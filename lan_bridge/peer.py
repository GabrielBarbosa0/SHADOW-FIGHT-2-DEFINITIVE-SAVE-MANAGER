from __future__ import annotations

import socket
import time
from dataclasses import dataclass
from enum import Enum

from .protocol import InputState, Packet, PacketType, ProtocolError, decode_packet, encode_packet


class PeerMode(str, Enum):
    HOST = "host"
    CLIENT = "client"


@dataclass(frozen=True, slots=True)
class LanEvent:
    name: str
    address: tuple[str, int] | None = None
    packet: Packet | None = None
    input_state: InputState | None = None
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
            if self.mode is PeerMode.CLIENT:
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
        if packet.kind is PacketType.DISCONNECT:
            self.connected = False
            self.peer_address = None
            return [LanEvent("disconnected", address, packet)]
        return [LanEvent("packet", address, packet)]

    def send_input(self, state: InputState) -> int:
        if not self.connected or self.peer_address is None:
            raise ConnectionError("Nenhum peer LAN conectado.")
        return self._send(PacketType.INPUT, state.encode(), self.peer_address)

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
