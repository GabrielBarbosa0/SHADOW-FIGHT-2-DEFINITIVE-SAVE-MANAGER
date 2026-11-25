from __future__ import annotations

import socket
import time
from dataclasses import dataclass
from enum import Enum

from .protocol import (
    FighterProfile,
    FightAuthority,
    FightConfig,
    FightStart,
    FightState,
    InputState,
    Packet,
    PacketType,
    PlayerRole,
    ProtocolError,
    decode_packet,
    decode_player_role,
    decode_round_id,
    encode_packet,
    encode_round_id,
)


class PeerMode(str, Enum):
    HOST = "host"
    CLIENT = "client"


@dataclass(frozen=True, slots=True)
class LanEvent:
    name: str
    address: tuple[str, int] | None = None
    packet: Packet | None = None
    input_state: InputState | None = None
    fighter_profile: FighterProfile | None = None
    fight_config: FightConfig | None = None
    fight_authority: FightAuthority | None = None
    fight_start: FightStart | None = None
    fight_state: FightState | None = None
    round_id: int | None = None
    player_role: PlayerRole | None = None
    detail: str = ""


class LanPeer:
    """Peer UDP pequeno, sem dependências, usado pelo protótipo e pelos testes."""

    HELLO_INTERVAL = 1.0
    PING_INTERVAL = 1.0
    PEER_TIMEOUT = 5.0

    def __init__(
        self,
        mode: PeerMode,
        token: str,
        *,
        bind_host: str = "0.0.0.0",
        port: int = 24190,
        server_host: str | None = None,
        device_name: str = "sf2-player",
    ) -> None:
        if mode is PeerMode.CLIENT and not server_host:
            raise ValueError("server_host é obrigatório no modo cliente.")
        if not 1 <= port <= 65535:
            raise ValueError("Porta UDP inválida.")
        encoded_name = device_name.encode("utf-8")
        if not encoded_name or len(encoded_name) > 64:
            raise ValueError("device_name deve ter entre 1 e 64 bytes.")

        self.mode = mode
        self.token = token
        self.device_name = encoded_name
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((bind_host, port if mode is PeerMode.HOST else 0))
        self.server_address = (server_host, port) if server_host else None
        self.peer_address: tuple[str, int] | None = None
        self.connected = False
        self._sequence = 0
        self._last_send = 0.0
        self._last_receive = 0.0

    @property
    def local_address(self) -> tuple[str, int]:
        host, port = self.socket.getsockname()
        return str(host), int(port)

    def close(self) -> None:
        if self.peer_address is not None:
            try:
                self._send(PacketType.DISCONNECT, address=self.peer_address)
            except OSError:
                pass
        self.socket.close()

    def start(self) -> None:
        if self.mode is PeerMode.CLIENT:
            self._send_hello()

    def tick(self, now: float | None = None) -> list[LanEvent]:
        now = time.monotonic() if now is None else now
        events: list[LanEvent] = []
        if self.connected and now - self._last_receive > self.PEER_TIMEOUT:
            self.connected = False
            old_peer = self.peer_address
            # A porta UDP de origem muda quando o app Android reinicia. Depois
            # do timeout, tanto cliente quanto host precisam liberar o endereco
            # antigo para que um novo HELLO autenticado possa reconectar.
            self.peer_address = None
            events.append(LanEvent("timeout", old_peer))
        if self.mode is PeerMode.CLIENT and not self.connected and now - self._last_send >= self.HELLO_INTERVAL:
            self._send_hello()
        elif self.connected and self.peer_address and now - self._last_send >= self.PING_INTERVAL:
            self._send(PacketType.PING, address=self.peer_address)
        return events

    def poll(self, timeout: float = 0.0) -> list[LanEvent]:
        self.socket.settimeout(timeout)
        try:
            data, raw_address = self.socket.recvfrom(2048)
        except (BlockingIOError, socket.timeout):
            return []
        address = (str(raw_address[0]), int(raw_address[1]))
        try:
            packet = decode_packet(data, self.token)
        except ProtocolError as exc:
            return [LanEvent("rejected", address, detail=str(exc))]

        if not self._address_allowed(address, packet.kind):
            return [LanEvent("rejected", address, packet, detail="Peer inesperado.")]

        self._last_receive = time.monotonic()
        if packet.kind is PacketType.HELLO and self.mode is PeerMode.HOST:
            self.peer_address = address
            self.connected = True
            self._send(PacketType.WELCOME, self.device_name, address)
            return [LanEvent("connected", address, packet, detail=packet.payload.decode("utf-8", "replace"))]
        if packet.kind is PacketType.WELCOME and self.mode is PeerMode.CLIENT:
            self.peer_address = address
            self.connected = True
            return [LanEvent("connected", address, packet, detail=packet.payload.decode("utf-8", "replace"))]
        if packet.kind is PacketType.PING:
            self._send(PacketType.PONG, address=address)
            return [LanEvent("ping", address, packet)]
        if packet.kind is PacketType.PONG:
            return [LanEvent("pong", address, packet)]
        if packet.kind is PacketType.INPUT:
            try:
                state = InputState.decode(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("input", address, packet, state)]
        if packet.kind is PacketType.PROFILE:
            try:
                profile = FighterProfile.decode(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("profile", address, packet, fighter_profile=profile)]
        if packet.kind is PacketType.READY:
            if packet.payload:
                return [LanEvent("rejected", address, packet, detail="READY deve ter payload vazio.")]
            return [LanEvent("ready", address, packet)]
        if packet.kind is PacketType.FIGHT_READY:
            if packet.payload:
                return [LanEvent("rejected", address, packet, detail="FIGHT_READY deve ter payload vazio.")]
            return [LanEvent("fight-ready", address, packet)]
        if packet.kind is PacketType.START:
            try:
                start = FightStart.decode(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("start", address, packet, fight_start=start, round_id=start.round_id)]
        if packet.kind is PacketType.START_ACK:
            try:
                round_id = decode_round_id(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("start-ack", address, packet, round_id=round_id)]
        if packet.kind is PacketType.STATE:
            try:
                state = FightState.decode(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("state", address, packet, fight_state=state, round_id=state.round_id)]
        if packet.kind is PacketType.PLAYER_ROLE:
            try:
                role = decode_player_role(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("player-role", address, packet, player_role=role)]
        if packet.kind is PacketType.FIGHT_CONFIG:
            try:
                config = FightConfig.decode(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent("fight-config", address, packet, fight_config=config)]
        if packet.kind is PacketType.FIGHT_AUTHORITY:
            try:
                authority = FightAuthority.decode(packet.payload)
            except ProtocolError as exc:
                return [LanEvent("rejected", address, packet, detail=str(exc))]
            return [LanEvent(
                "fight-authority", address, packet,
                fight_authority=authority, round_id=authority.round_id,
            )]
        if packet.kind is PacketType.DISCONNECT:
            self.connected = False
            self.peer_address = None
            return [LanEvent("disconnected", address, packet)]
        return [LanEvent("packet", address, packet)]

    def send_input(self, state: InputState) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.INPUT, state.encode(), self.peer_address)

    def send_profile(self, profile: FighterProfile) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.PROFILE, profile.encode(), self.peer_address)

    def send_ready(self) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.READY, address=self.peer_address)

    def send_fight_ready(self) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.FIGHT_READY, address=self.peer_address)

    def send_start(self, start: FightStart) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.START, start.encode(), self.peer_address)

    def send_start_ack(self, round_id: int) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.START_ACK, encode_round_id(round_id), self.peer_address)

    def send_state(self, state: FightState) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.STATE, state.encode(), self.peer_address)

    def send_fight_config(self, config: FightConfig) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.FIGHT_CONFIG, config.encode(), self.peer_address)

    def send_fight_authority(self, authority: FightAuthority) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.FIGHT_AUTHORITY, authority.encode(), self.peer_address)

    def _address_allowed(self, address: tuple[str, int], kind: PacketType) -> bool:
        if self.mode is PeerMode.HOST:
            return self.peer_address in (None, address) and (self.peer_address is not None or kind is PacketType.HELLO)
        assert self.server_address is not None
        expected_ip = socket.gethostbyname(self.server_address[0])
        return address == (expected_ip, self.server_address[1])

    def _send_hello(self) -> None:
        assert self.server_address is not None
        self._send(PacketType.HELLO, self.device_name, self.server_address)

    def _send(self, kind: PacketType, payload: bytes = b"", address: tuple[str, int] | None = None) -> int:
        if address is None:
            raise ConnectionError("Endereço LAN ainda não definido.")
        sequence = self._sequence
        packet = encode_packet(kind, sequence, self.token, payload)
        self.socket.sendto(packet, address)
        self._sequence = (self._sequence + 1) & 0xFFFFFFFF
        self._last_send = time.monotonic()
        return sequence
