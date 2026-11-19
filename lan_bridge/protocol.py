from __future__ import annotations

import hashlib
import math
import struct
import time
from dataclasses import dataclass
from enum import IntEnum, IntFlag

MAGIC = b"SF2L"
VERSION = 1
MAX_PAYLOAD = 1024
HEADER = struct.Struct("!4sBBHIQ16s")
INPUT_PAYLOAD = struct.Struct("!ffH")
PROFILE_HEADER = struct.Struct("!BB")
PROFILE_VERSION = 1
PROFILE_FIELDS = ("name", "avatar", "voice", "skeleton", "armor", "helm", "weapon", "ranged", "magic")
MAX_PROFILE_FIELD_BYTES = 96


class ProtocolError(ValueError):
    """Pacote LAN inválido ou pertencente a outra sessão."""


class PacketType(IntEnum):
    HELLO = 1
    WELCOME = 2
    PING = 3
    PONG = 4
    INPUT = 5
    DISCONNECT = 6
    PROFILE = 7
    READY = 8


class Button(IntFlag):
    NONE = 0
    PUNCH = 1 << 0
    KICK = 1 << 1
    RANGED = 1 << 2
    MAGIC = 1 << 3
    BOSS_ABILITY = 1 << 4
    PAUSE = 1 << 5


@dataclass(frozen=True, slots=True)
class Packet:
    kind: PacketType
    sequence: int
    timestamp_ms: int
    payload: bytes = b""


@dataclass(frozen=True, slots=True)
class InputState:
    horizontal: float = 0.0
    vertical: float = 0.0
    buttons: Button = Button.NONE

    def encode(self) -> bytes:
        if not math.isfinite(self.horizontal) or not -1.0 <= self.horizontal <= 1.0:
            raise ProtocolError("Eixo horizontal fora do intervalo -1..1.")
        if not math.isfinite(self.vertical) or not -1.0 <= self.vertical <= 1.0:
            raise ProtocolError("Eixo vertical fora do intervalo -1..1.")
        return INPUT_PAYLOAD.pack(self.horizontal, self.vertical, int(self.buttons))

    @classmethod
    def decode(cls, payload: bytes) -> InputState:
        if len(payload) != INPUT_PAYLOAD.size:
            raise ProtocolError("Tamanho inválido do estado de entrada.")
        horizontal, vertical, buttons = INPUT_PAYLOAD.unpack(payload)
        state = cls(horizontal, vertical, Button(buttons))
        state.encode()
        return state


@dataclass(frozen=True, slots=True)
class FighterProfile:
    level: int
    name: str
    avatar: str
    voice: str
    skeleton: str
    armor: str
    helm: str
    weapon: str
    ranged: str
    magic: str

    def encode(self) -> bytes:
        if not 1 <= self.level <= 255:
            raise ProtocolError("Nivel do lutador fora do intervalo 1..255.")
        payload = bytearray(PROFILE_HEADER.pack(PROFILE_VERSION, self.level))
        for field_name in PROFILE_FIELDS:
            encoded = getattr(self, field_name).encode("utf-8")
            if len(encoded) > MAX_PROFILE_FIELD_BYTES:
                raise ProtocolError(f"Campo de perfil muito longo: {field_name}.")
            payload.append(len(encoded))
            payload.extend(encoded)
        return bytes(payload)

    @classmethod
    def decode(cls, payload: bytes) -> FighterProfile:
        if len(payload) < PROFILE_HEADER.size:
            raise ProtocolError("Perfil do lutador incompleto.")
        version, level = PROFILE_HEADER.unpack_from(payload)
        if version != PROFILE_VERSION:
            raise ProtocolError(f"Versao de perfil incompativel: {version}.")
        offset = PROFILE_HEADER.size
        values: dict[str, str] = {}
        for field_name in PROFILE_FIELDS:
            if offset >= len(payload):
                raise ProtocolError("Perfil do lutador truncado.")
            size = payload[offset]
            offset += 1
            if size > MAX_PROFILE_FIELD_BYTES or offset + size > len(payload):
                raise ProtocolError(f"Campo de perfil invalido: {field_name}.")
            try:
                values[field_name] = payload[offset:offset + size].decode("utf-8")
            except UnicodeDecodeError as exc:
                raise ProtocolError(f"Campo de perfil nao e UTF-8: {field_name}.") from exc
            offset += size
        if offset != len(payload):
            raise ProtocolError("Perfil do lutador contem bytes excedentes.")
        return cls(level=level, **values)


def session_digest(token: str) -> bytes:
    if not token or len(token.encode("utf-8")) > 128:
        raise ProtocolError("O token da sala deve ter entre 1 e 128 bytes.")
    # SHA-256 existe tanto no Python quanto no Android sem bibliotecas extras.
    return hashlib.sha256(token.encode("utf-8")).digest()[:16]


def encode_packet(
    kind: PacketType,
    sequence: int,
    token: str,
    payload: bytes = b"",
    *,
    timestamp_ms: int | None = None,
) -> bytes:
    if not 0 <= sequence <= 0xFFFFFFFF:
        raise ProtocolError("Sequência fora do intervalo uint32.")
    if len(payload) > MAX_PAYLOAD:
        raise ProtocolError(f"Payload maior que {MAX_PAYLOAD} bytes.")
    if timestamp_ms is None:
        timestamp_ms = int(time.time() * 1000)
    if not 0 <= timestamp_ms <= 0xFFFFFFFFFFFFFFFF:
        raise ProtocolError("Timestamp fora do intervalo uint64.")
    return HEADER.pack(
        MAGIC,
        VERSION,
        int(kind),
        len(payload),
        sequence,
        timestamp_ms,
        session_digest(token),
    ) + payload


def decode_packet(data: bytes, token: str) -> Packet:
    if len(data) < HEADER.size:
        raise ProtocolError("Datagrama menor que o cabeçalho SF2 LAN.")
    magic, version, raw_kind, payload_size, sequence, timestamp_ms, digest = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise ProtocolError("Assinatura SF2 LAN ausente.")
    if version != VERSION:
        raise ProtocolError(f"Versão de protocolo incompatível: {version}.")
    try:
        kind = PacketType(raw_kind)
    except ValueError as exc:
        raise ProtocolError(f"Tipo de pacote desconhecido: {raw_kind}.") from exc
    if payload_size > MAX_PAYLOAD or len(data) != HEADER.size + payload_size:
        raise ProtocolError("Tamanho do payload não corresponde ao cabeçalho.")
    if digest != session_digest(token):
        raise ProtocolError("Token da sala incorreto.")
    return Packet(kind, sequence, timestamp_ms, data[HEADER.size:])
