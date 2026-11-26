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
FIGHT_START_PAYLOAD = struct.Struct("!II")
ROUND_ID_PAYLOAD = struct.Struct("!I")
FIGHT_STATE_PAYLOAD = struct.Struct("!IIffffffff")
FIGHT_AUTHORITY_PAYLOAD = struct.Struct("!IIffIIffffff")
PLAYER_ROLE_PAYLOAD = struct.Struct("!B")
FIGHT_CONFIG_HEADER = struct.Struct("!BBQH")
FIGHT_CONFIG_VERSION = 1
MAX_FIGHT_CONFIG_ID_BYTES = 512
MATCH_CONFIG_HEADER = struct.Struct("!BBQBB")
MATCH_CONFIG_ACK_PAYLOAD = struct.Struct("!Q")
MATCH_CONFIG_VERSION = 1
MAX_PRESET_NAME_BYTES = 32
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
    FIGHT_READY = 9
    START = 10
    START_ACK = 11
    STATE = 12
    PLAYER_ROLE = 13
    FIGHT_CONFIG = 14
    FIGHT_AUTHORITY = 15
    MATCH_CONFIG = 16
    MATCH_CONFIG_ACK = 17


class PlayerRole(IntEnum):
    PLAYER_ONE = 1
    PLAYER_TWO = 2


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


@dataclass(frozen=True, slots=True)
class FightStart:
    round_id: int
    delay_ms: int

    def encode(self) -> bytes:
        if not 1 <= self.round_id <= 0xFFFFFFFF:
            raise ProtocolError("Identificador da rodada fora do intervalo uint32.")
        if not 500 <= self.delay_ms <= 10_000:
            raise ProtocolError("Atraso de inicio deve ficar entre 500 e 10000 ms.")
        return FIGHT_START_PAYLOAD.pack(self.round_id, self.delay_ms)

    @classmethod
    def decode(cls, payload: bytes) -> FightStart:
        if len(payload) != FIGHT_START_PAYLOAD.size:
            raise ProtocolError("Tamanho invalido do sinal START.")
        value = cls(*FIGHT_START_PAYLOAD.unpack(payload))
        value.encode()
        return value


@dataclass(frozen=True, slots=True)
class FightConfig:
    """Identidade da luta escolhida pelo jogo antes da injecao LAN."""

    mode: int
    fingerprint: int
    identity: str

    def encode(self) -> bytes:
        if not 0 <= self.mode <= 0xFF:
            raise ProtocolError("Modo da luta fora do intervalo uint8.")
        if not 1 <= self.fingerprint <= 0xFFFFFFFFFFFFFFFF:
            raise ProtocolError("Fingerprint da luta fora do intervalo uint64.")
        encoded_identity = self.identity.encode("utf-8")
        if not encoded_identity or len(encoded_identity) > MAX_FIGHT_CONFIG_ID_BYTES:
            raise ProtocolError(
                f"Identidade da luta deve ter entre 1 e {MAX_FIGHT_CONFIG_ID_BYTES} bytes."
            )
        return FIGHT_CONFIG_HEADER.pack(
            FIGHT_CONFIG_VERSION, self.mode, self.fingerprint, len(encoded_identity)
        ) + encoded_identity

    @classmethod
    def decode(cls, payload: bytes) -> FightConfig:
        if len(payload) < FIGHT_CONFIG_HEADER.size:
            raise ProtocolError("Configuracao da luta incompleta.")
        version, mode, fingerprint, identity_size = FIGHT_CONFIG_HEADER.unpack_from(payload)
        if version != FIGHT_CONFIG_VERSION:
            raise ProtocolError(f"Versao de configuracao de luta incompativel: {version}.")
        if identity_size > MAX_FIGHT_CONFIG_ID_BYTES or len(payload) != FIGHT_CONFIG_HEADER.size + identity_size:
            raise ProtocolError("Tamanho da identidade da luta invalido.")
        try:
            identity = payload[FIGHT_CONFIG_HEADER.size:].decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ProtocolError("Identidade da luta nao e UTF-8.") from exc
        value = cls(mode, fingerprint, identity)
        value.encode()
        return value


@dataclass(frozen=True, slots=True)
class MatchConfig:
    """Escolha autoritativa de tier e equipamento para os dois jogadores."""

    match_level: int
    seed: int
    player_one_preset: str
    player_two_preset: str

    def encode(self) -> bytes:
        if not 1 <= self.match_level <= 52:
            raise ProtocolError("Nivel da partida deve ficar entre 1 e 52.")
        if not 1 <= self.seed <= 0xFFFFFFFFFFFFFFFF:
            raise ProtocolError("Seed da partida fora do intervalo uint64.")
        player_one = self.player_one_preset.encode("utf-8")
        player_two = self.player_two_preset.encode("utf-8")
        for name, encoded in (("P1", player_one), ("P2", player_two)):
            if not encoded or len(encoded) > MAX_PRESET_NAME_BYTES:
                raise ProtocolError(
                    f"Preset de {name} deve ter entre 1 e {MAX_PRESET_NAME_BYTES} bytes."
                )
        return MATCH_CONFIG_HEADER.pack(
            MATCH_CONFIG_VERSION,
            self.match_level,
            self.seed,
            len(player_one),
            len(player_two),
        ) + player_one + player_two

    @property
    def fingerprint(self) -> int:
        value = int.from_bytes(hashlib.sha256(self.encode()).digest()[:8], "big")
        return value or 1

    @classmethod
    def decode(cls, payload: bytes) -> MatchConfig:
        if len(payload) < MATCH_CONFIG_HEADER.size:
            raise ProtocolError("Configuracao da partida incompleta.")
        version, match_level, seed, p1_size, p2_size = MATCH_CONFIG_HEADER.unpack_from(payload)
        if version != MATCH_CONFIG_VERSION:
            raise ProtocolError(f"Versao de configuracao de partida incompativel: {version}.")
        expected_size = MATCH_CONFIG_HEADER.size + p1_size + p2_size
        if (
            p1_size < 1
            or p2_size < 1
            or p1_size > MAX_PRESET_NAME_BYTES
            or p2_size > MAX_PRESET_NAME_BYTES
            or len(payload) != expected_size
        ):
            raise ProtocolError("Tamanho dos presets da partida invalido.")
        offset = MATCH_CONFIG_HEADER.size
        try:
            player_one = payload[offset:offset + p1_size].decode("utf-8")
            offset += p1_size
            player_two = payload[offset:offset + p2_size].decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ProtocolError("Preset da partida nao e UTF-8.") from exc
        value = cls(match_level, seed, player_one, player_two)
        value.encode()
        return value


def encode_match_config_ack(fingerprint: int) -> bytes:
    if not 1 <= fingerprint <= 0xFFFFFFFFFFFFFFFF:
        raise ProtocolError("Fingerprint da partida fora do intervalo uint64.")
    return MATCH_CONFIG_ACK_PAYLOAD.pack(fingerprint)


def decode_match_config_ack(payload: bytes) -> int:
    if len(payload) != MATCH_CONFIG_ACK_PAYLOAD.size:
        raise ProtocolError("Tamanho invalido da confirmacao da partida.")
    fingerprint = MATCH_CONFIG_ACK_PAYLOAD.unpack(payload)[0]
    encode_match_config_ack(fingerprint)
    return fingerprint


@dataclass(frozen=True, slots=True)
class FightState:
    """Snapshot read-only do HUD capturado no thread do ``FixedUpdate``."""

    round_id: int
    tick: int
    time_left_seconds: float
    time_left_frames: float
    left_life: float
    left_hit_life: float
    right_life: float
    right_hit_life: float
    time_passed_frames: float
    time_total_frames: float

    def encode(self) -> bytes:
        if not 1 <= self.round_id <= 0xFFFFFFFF:
            raise ProtocolError("Identificador da rodada fora do intervalo uint32.")
        if not 1 <= self.tick <= 0xFFFFFFFF:
            raise ProtocolError("Tick do estado fora do intervalo uint32.")
        values = (
            self.time_left_seconds,
            self.time_left_frames,
            self.left_life,
            self.left_hit_life,
            self.right_life,
            self.right_hit_life,
            self.time_passed_frames,
            self.time_total_frames,
        )
        if any(not math.isfinite(value) or abs(value) > 1_000_000 for value in values):
            raise ProtocolError("Snapshot da luta contem valor float invalido.")
        return FIGHT_STATE_PAYLOAD.pack(self.round_id, self.tick, *values)

    @classmethod
    def decode(cls, payload: bytes) -> FightState:
        if len(payload) != FIGHT_STATE_PAYLOAD.size:
            raise ProtocolError("Tamanho invalido do snapshot da luta.")
        value = cls(*FIGHT_STATE_PAYLOAD.unpack(payload))
        value.encode()
        return value


@dataclass(frozen=True, slots=True)
class FightAuthority:
    """Estado de combate decidido pelo jogador 1.

    ``damage`` e a fracao acumulada de dano (0..1), enquanto ``rounds_lost``
    e o contador interno incrementado quando o combatente chega a 1.
    """

    round_id: int
    tick: int
    player_damage: float
    opponent_damage: float
    player_rounds_lost: int
    opponent_rounds_lost: int
    player_x: float = 0.0
    player_y: float = 0.0
    player_z: float = 0.0
    opponent_x: float = 0.0
    opponent_y: float = 0.0
    opponent_z: float = 0.0

    def encode(self) -> bytes:
        if not 1 <= self.round_id <= 0xFFFFFFFF:
            raise ProtocolError("Identificador da rodada fora do intervalo uint32.")
        if not 1 <= self.tick <= 0xFFFFFFFF:
            raise ProtocolError("Tick da autoridade fora do intervalo uint32.")
        for value in (self.player_damage, self.opponent_damage):
            if not math.isfinite(value) or not 0.0 <= value <= 1.0:
                raise ProtocolError("Dano autoritativo fora do intervalo 0..1.")
        for value in (self.player_rounds_lost, self.opponent_rounds_lost):
            if not 0 <= value <= 2:
                raise ProtocolError("Contador autoritativo de rounds fora do intervalo 0..2.")
        positions = (
            self.player_x, self.player_y, self.player_z,
            self.opponent_x, self.opponent_y, self.opponent_z,
        )
        if any(not math.isfinite(value) or abs(value) > 1_000_000.0 for value in positions):
            raise ProtocolError("Posicao autoritativa fora do intervalo seguro.")
        return FIGHT_AUTHORITY_PAYLOAD.pack(
            self.round_id,
            self.tick,
            self.player_damage,
            self.opponent_damage,
            self.player_rounds_lost,
            self.opponent_rounds_lost,
            *positions,
        )

    @classmethod
    def decode(cls, payload: bytes) -> FightAuthority:
        if len(payload) != FIGHT_AUTHORITY_PAYLOAD.size:
            raise ProtocolError("Tamanho invalido do estado autoritativo da luta.")
        value = cls(*FIGHT_AUTHORITY_PAYLOAD.unpack(payload))
        value.encode()
        return value


@dataclass(frozen=True, slots=True)
class FightStateComparison:
    """Divergencia entre duas simulacoes, normalizada pela identidade dos lutadores."""

    round_id: int
    local_tick: int
    remote_tick: int
    remote_age_ms: int
    timer_frames_delta: float
    elapsed_frames_delta: float
    local_fighter_life_delta: float
    local_fighter_hit_delta: float
    remote_fighter_life_delta: float
    remote_fighter_hit_delta: float
    max_life_delta: float
    within_tolerance: bool


def compare_fight_states(
    local: FightState,
    remote: FightState,
    *,
    remote_age_ms: int = 0,
    frame_tolerance: float = 12.0,
    life_tolerance: float = 0.01,
    freshness_tolerance_ms: int = 500,
) -> FightStateComparison:
    """Compara peers que exibem P1 à esquerda e P2 à direita nas duas telas."""

    if local.round_id != remote.round_id:
        raise ProtocolError("Snapshots pertencem a rodadas diferentes.")
    if remote_age_ms < 0:
        raise ProtocolError("Idade do snapshot remoto nao pode ser negativa.")
    if frame_tolerance < 0 or life_tolerance < 0 or freshness_tolerance_ms < 0:
        raise ProtocolError("Tolerancias de comparacao nao podem ser negativas.")

    timer_frames_delta = local.time_left_frames - remote.time_left_frames
    elapsed_frames_delta = local.time_passed_frames - remote.time_passed_frames
    local_fighter_life_delta = local.left_life - remote.left_life
    local_fighter_hit_delta = local.left_hit_life - remote.left_hit_life
    remote_fighter_life_delta = local.right_life - remote.right_life
    remote_fighter_hit_delta = local.right_hit_life - remote.right_hit_life
    max_life_delta = max(
        abs(local_fighter_life_delta),
        abs(local_fighter_hit_delta),
        abs(remote_fighter_life_delta),
        abs(remote_fighter_hit_delta),
    )
    within_tolerance = (
        remote_age_ms <= freshness_tolerance_ms
        and abs(timer_frames_delta) <= frame_tolerance
        and abs(elapsed_frames_delta) <= frame_tolerance
        and max_life_delta <= life_tolerance
    )
    return FightStateComparison(
        round_id=local.round_id,
        local_tick=local.tick,
        remote_tick=remote.tick,
        remote_age_ms=remote_age_ms,
        timer_frames_delta=timer_frames_delta,
        elapsed_frames_delta=elapsed_frames_delta,
        local_fighter_life_delta=local_fighter_life_delta,
        local_fighter_hit_delta=local_fighter_hit_delta,
        remote_fighter_life_delta=remote_fighter_life_delta,
        remote_fighter_hit_delta=remote_fighter_hit_delta,
        max_life_delta=max_life_delta,
        within_tolerance=within_tolerance,
    )


def encode_round_id(round_id: int) -> bytes:
    if not 1 <= round_id <= 0xFFFFFFFF:
        raise ProtocolError("Identificador da rodada fora do intervalo uint32.")
    return ROUND_ID_PAYLOAD.pack(round_id)


def decode_round_id(payload: bytes) -> int:
    if len(payload) != ROUND_ID_PAYLOAD.size:
        raise ProtocolError("Tamanho invalido do identificador de rodada.")
    round_id = ROUND_ID_PAYLOAD.unpack(payload)[0]
    encode_round_id(round_id)
    return round_id


def encode_player_role(role: PlayerRole | int) -> bytes:
    try:
        value = PlayerRole(role)
    except ValueError as exc:
        raise ProtocolError("Papel de jogador invalido.") from exc
    return PLAYER_ROLE_PAYLOAD.pack(int(value))


def decode_player_role(payload: bytes) -> PlayerRole:
    if len(payload) != PLAYER_ROLE_PAYLOAD.size:
        raise ProtocolError("Tamanho invalido do papel de jogador.")
    try:
        return PlayerRole(PLAYER_ROLE_PAYLOAD.unpack(payload)[0])
    except ValueError as exc:
        raise ProtocolError("Papel de jogador invalido.") from exc


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
