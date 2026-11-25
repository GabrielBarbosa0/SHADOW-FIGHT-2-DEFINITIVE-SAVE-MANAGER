from __future__ import annotations

import socket
import select
import time
from dataclasses import dataclass

from .protocol import (
    FightConfig,
    FightStart,
    Packet,
    PacketType,
    ProtocolError,
    decode_packet,
    encode_player_role,
    decode_round_id,
    encode_packet,
)


@dataclass(frozen=True, slots=True)
class RelayEvent:
    name: str
    address: tuple[str, int]
    packet: Packet | None = None
    detail: str = ""


class LanRoomRelay:
    """Relay UDP autenticado para dois clientes separados por NAT.

    O relay termina HELLO e heartbeat. Por padrao, os demais pacotes sao
    encaminhados sem alteracao. Quando ``fight_start_delay_ms`` e informado, o
    relay tambem coordena START/START_ACK para duas instancias Android em modo
    client, que nao possuem entre si uma autoridade capaz de iniciar a rodada.
    """

    PEER_TIMEOUT = 15.0

    def __init__(
        self,
        token: str,
        *,
        bind_host: str = "0.0.0.0",
        port: int = 24190,
        second_port: int | None = None,
        name: str = "sf2-lan-relay",
        fight_start_delay_ms: int | None = None,
    ) -> None:
        if not 1 <= port <= 65535:
            raise ValueError("Porta UDP invalida.")
        if second_port is not None and (not 1 <= second_port <= 65535 or second_port == port):
            raise ValueError("A segunda porta UDP deve ser valida e diferente da primeira.")
        if fight_start_delay_ms is not None and not 500 <= fight_start_delay_ms <= 10_000:
            raise ValueError("fight_start_delay_ms deve ficar entre 500 e 10000.")
        encoded_name = name.encode("utf-8")
        if not encoded_name or len(encoded_name) > 64:
            raise ValueError("name deve ter entre 1 e 64 bytes.")

        self.token = token
        self.name = encoded_name
        self.sockets: list[socket.socket] = []
        for listen_port in (port,) if second_port is None else (port, second_port):
            listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind((bind_host, listen_port))
            self.sockets.append(listener)
        self.socket = self.sockets[0]
        self._participants: dict[tuple[int, tuple[str, int]], float] = {}
        self._sequence = 0
        self.fight_start_delay_ms = fight_start_delay_ms
        self._fight_ready: set[tuple[int, tuple[str, int]]] = set()
        self._start_acked: set[tuple[int, tuple[str, int]]] = set()
        self._active_start: FightStart | None = None
        self._fight_configs: dict[tuple[int, tuple[str, int]], FightConfig] = {}
        self._start_sent_at = 0.0
        self._last_start_send = 0.0

    @property
    def local_address(self) -> tuple[str, int]:
        host, port = self.socket.getsockname()
        return str(host), int(port)

    @property
    def local_addresses(self) -> tuple[tuple[str, int], ...]:
        return tuple((str(sock.getsockname()[0]), int(sock.getsockname()[1])) for sock in self.sockets)

    @property
    def participants(self) -> tuple[tuple[str, int], ...]:
        return tuple(endpoint[1] for endpoint in self._participants)

    def close(self) -> None:
        for endpoint in tuple(self._participants):
            try:
                self._send(PacketType.DISCONNECT, endpoint)
            except OSError:
                pass
        self._participants.clear()
        self._reset_fight()
        for listener in self.sockets:
            listener.close()

    def poll(self, timeout: float = 0.0) -> list[RelayEvent]:
        readable, _, _ = select.select(self.sockets, [], [], timeout)
        if not readable:
            return []
        listener = readable[0]
        socket_index = self.sockets.index(listener)
        try:
            data, raw_address = listener.recvfrom(2048)
        except (BlockingIOError, socket.timeout, ConnectionResetError):
            # No Windows, um cliente UDP encerrado pode produzir WSAECONNRESET
            # no listener. Isso nao deve derrubar a sala do outro jogador.
            return []

        address = (str(raw_address[0]), int(raw_address[1]))
        endpoint = (socket_index, address)
        try:
            packet = decode_packet(data, self.token)
        except ProtocolError as exc:
            return [RelayEvent("rejected", address, detail=str(exc))]

        now = time.monotonic()
        if packet.kind is PacketType.HELLO:
            if endpoint not in self._participants and len(self._participants) >= 2:
                return [RelayEvent("room-full", address, packet, "A sala ja possui dois jogadores.")]
            joined = endpoint not in self._participants
            self._participants[endpoint] = now
            self._send(PacketType.WELCOME, endpoint, self.name)
            player = self._player_number(endpoint)
            self._send(PacketType.PLAYER_ROLE, endpoint, encode_player_role(player))
            return [RelayEvent(
                "joined" if joined else "hello", address, packet,
                f"player={player} listener={self.local_addresses[socket_index][1]}",
            )]

        if endpoint not in self._participants:
            return [RelayEvent("rejected", address, packet, "Jogador ainda nao enviou HELLO.")]
        self._participants[endpoint] = now

        if packet.kind is PacketType.PING:
            self._send(PacketType.PONG, endpoint)
            return [RelayEvent("heartbeat", address, packet)]
        if packet.kind is PacketType.DISCONNECT:
            self._forward(data, endpoint)
            del self._participants[endpoint]
            self._reset_fight()
            return [RelayEvent("left", address, packet)]
        if packet.kind in (PacketType.WELCOME, PacketType.PONG):
            return [RelayEvent("ignored", address, packet)]

        if self.fight_start_delay_ms is not None and packet.kind is PacketType.FIGHT_READY:
            return [self._handle_fight_ready(data, endpoint, packet, now)]
        if self.fight_start_delay_ms is not None and packet.kind is PacketType.FIGHT_CONFIG:
            return [self._handle_fight_config(data, endpoint, packet)]
        if self.fight_start_delay_ms is not None and packet.kind is PacketType.START_ACK:
            return [self._handle_start_ack(endpoint, packet)]

        forwarded = self._forward(data, endpoint)
        return [RelayEvent("forwarded" if forwarded else "waiting", address, packet, packet.kind.name)]

    def tick(self, now: float | None = None) -> list[RelayEvent]:
        now = time.monotonic() if now is None else now
        events: list[RelayEvent] = []
        for endpoint, last_seen in tuple(self._participants.items()):
            if now - last_seen <= self.PEER_TIMEOUT:
                continue
            del self._participants[endpoint]
            self._reset_fight()
            events.append(RelayEvent("timeout", endpoint[1]))
        if (
            self._active_start is not None
            and len(self._start_acked) < len(self._participants)
            and now - self._last_start_send >= 0.25
        ):
            for endpoint in self._participants:
                if endpoint not in self._start_acked:
                    self._send(PacketType.START, endpoint, self._active_start.encode())
            self._last_start_send = now
        return events

    def _handle_fight_ready(
        self,
        data: bytes,
        endpoint: tuple[int, tuple[str, int]],
        packet: Packet,
        now: float,
    ) -> RelayEvent:
        # Depois de uma rodada confirmada, um novo FIGHT_READY muito proximo
        # ainda pode ser uma retransmissao UDP em voo. A janela impede que ela
        # crie uma segunda rodada acidental; apos isso, inicia um novo ciclo.
        if (
            self._active_start is not None
            and len(self._start_acked) == len(self._participants)
            and now - self._start_sent_at > 5.0
        ):
            self._reset_fight()

        forwarded = self._forward(data, endpoint)
        self._fight_ready.add(endpoint)

        if len(self._fight_configs) < 2:
            return RelayEvent("fight-config-wait", endpoint[1], packet, "Aguardando configuracao das duas arenas.")
        if len(set(self._fight_configs.values())) != 1:
            return RelayEvent("fight-config-mismatch", endpoint[1], packet, self._fight_config_detail())

        if self._active_start is not None:
            if endpoint not in self._start_acked:
                self._send(PacketType.START, endpoint, self._active_start.encode())
                self._last_start_send = now
                return RelayEvent(
                    "start-retry", endpoint[1], packet,
                    f"round={self._active_start.round_id}",
                )
            return RelayEvent("fight-ready-duplicate", endpoint[1], packet)

        participants = set(self._participants)
        if len(participants) == 2 and participants.issubset(self._fight_ready):
            round_id = int(time.time_ns() & 0x7FFFFFFF) or 1
            self._active_start = FightStart(round_id, self.fight_start_delay_ms or 2000)
            payload = self._active_start.encode()
            for participant in self._participants:
                self._send(PacketType.START, participant, payload)
            self._start_sent_at = now
            self._last_start_send = now
            return RelayEvent(
                "started", endpoint[1], packet,
                f"round={round_id} delay={self._active_start.delay_ms}ms",
            )

        return RelayEvent(
            "forwarded" if forwarded else "waiting",
            endpoint[1], packet, packet.kind.name,
        )

    def _handle_fight_config(
        self,
        data: bytes,
        endpoint: tuple[int, tuple[str, int]],
        packet: Packet,
    ) -> RelayEvent:
        try:
            config = FightConfig.decode(packet.payload)
        except ProtocolError as exc:
            return RelayEvent("rejected", endpoint[1], packet, str(exc))
        self._fight_configs[endpoint] = config
        forwarded = self._forward(data, endpoint)
        if len(self._fight_configs) < 2:
            return RelayEvent("fight-config", endpoint[1], packet, f"aguardando peer: {config.identity}")
        if len(set(self._fight_configs.values())) != 1:
            self._fight_ready.clear()
            return RelayEvent("fight-config-mismatch", endpoint[1], packet, self._fight_config_detail())
        return RelayEvent(
            "fight-config-matched", endpoint[1], packet,
            f"fingerprint={config.fingerprint:016x} identity={config.identity}",
        )

    def _fight_config_detail(self) -> str:
        values = []
        for endpoint, config in self._fight_configs.items():
            player = self._player_number(endpoint)
            values.append(
                f"P{player}=mode:{config.mode},fingerprint:{config.fingerprint:016x},identity:{config.identity}"
            )
        return " | ".join(values)

    def _player_number(self, endpoint: tuple[int, tuple[str, int]]) -> int:
        # No modo de duas portas, cada porta pertence permanentemente a uma VM.
        # Assim P1/P2 nao invertem se os Androids reconectarem em outra ordem.
        if len(self.sockets) == 2:
            return endpoint[0] + 1
        return tuple(self._participants).index(endpoint) + 1

    def _handle_start_ack(
        self,
        endpoint: tuple[int, tuple[str, int]],
        packet: Packet,
    ) -> RelayEvent:
        try:
            round_id = decode_round_id(packet.payload)
        except ProtocolError as exc:
            return RelayEvent("rejected", endpoint[1], packet, str(exc))
        if self._active_start is None or round_id != self._active_start.round_id:
            return RelayEvent("rejected", endpoint[1], packet, "START_ACK de rodada desconhecida.")
        self._start_acked.add(endpoint)
        confirmed = len(self._start_acked) == len(self._participants) == 2
        return RelayEvent(
            "start-confirmed" if confirmed else "start-ack",
            endpoint[1], packet,
            f"round={round_id} acknowledgements={len(self._start_acked)}/2",
        )

    def _reset_fight(self) -> None:
        self._fight_ready.clear()
        self._fight_configs.clear()
        self._start_acked.clear()
        self._active_start = None
        self._start_sent_at = 0.0
        self._last_start_send = 0.0

    def _forward(self, data: bytes, sender: tuple[int, tuple[str, int]]) -> bool:
        forwarded = False
        for socket_index, address in self._participants:
            if (socket_index, address) != sender:
                self.sockets[socket_index].sendto(data, address)
                forwarded = True
        return forwarded

    def _send(
        self,
        kind: PacketType,
        endpoint: tuple[int, tuple[str, int]],
        payload: bytes = b"",
    ) -> None:
        packet = encode_packet(kind, self._sequence, self.token, payload)
        socket_index, address = endpoint
        self.sockets[socket_index].sendto(packet, address)
        self._sequence = (self._sequence + 1) & 0xFFFFFFFF
