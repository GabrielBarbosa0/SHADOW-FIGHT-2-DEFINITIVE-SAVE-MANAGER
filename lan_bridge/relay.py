from __future__ import annotations

import socket
import select
import time
from dataclasses import dataclass

from .protocol import Packet, PacketType, ProtocolError, decode_packet, encode_packet


@dataclass(frozen=True, slots=True)
class RelayEvent:
    name: str
    address: tuple[str, int]
    packet: Packet | None = None
    detail: str = ""


class LanRoomRelay:
    """Relay UDP autenticado para dois clientes separados por NAT.

    O relay termina apenas o HELLO e o heartbeat. PROFILE, READY e INPUT sao
    encaminhados sem alteracao, preservando sequencia e payload de cada jogador.
    """

    PEER_TIMEOUT = 8.0

    def __init__(
        self,
        token: str,
        *,
        bind_host: str = "0.0.0.0",
        port: int = 24190,
        second_port: int | None = None,
        name: str = "sf2-lan-relay",
    ) -> None:
        if not 1 <= port <= 65535:
            raise ValueError("Porta UDP invalida.")
        if second_port is not None and (not 1 <= second_port <= 65535 or second_port == port):
            raise ValueError("A segunda porta UDP deve ser valida e diferente da primeira.")
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
            player = tuple(self._participants).index(endpoint) + 1
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
            return [RelayEvent("left", address, packet)]
        if packet.kind in (PacketType.WELCOME, PacketType.PONG):
            return [RelayEvent("ignored", address, packet)]

        forwarded = self._forward(data, endpoint)
        return [RelayEvent("forwarded" if forwarded else "waiting", address, packet, packet.kind.name)]

    def tick(self, now: float | None = None) -> list[RelayEvent]:
        now = time.monotonic() if now is None else now
        events: list[RelayEvent] = []
        for endpoint, last_seen in tuple(self._participants.items()):
            if now - last_seen <= self.PEER_TIMEOUT:
                continue
            del self._participants[endpoint]
            events.append(RelayEvent("timeout", endpoint[1]))
        return events

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
