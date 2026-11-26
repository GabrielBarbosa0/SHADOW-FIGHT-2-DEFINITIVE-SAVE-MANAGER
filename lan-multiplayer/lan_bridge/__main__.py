from __future__ import annotations

import argparse
import time
from dataclasses import replace

from .equipment import EQUIPMENT_PRESETS
from .peer import LanPeer, PeerMode
from .protocol import Button, FighterProfile, FightStart, FightState, InputState, PacketType
from .relay import LanRoomRelay


def profile_with_equipment(profile: FighterProfile, preset_name: str) -> FighterProfile:
    preset = EQUIPMENT_PRESETS[preset_name]
    return replace(
        profile,
        avatar=preset["Avatar"],
        skeleton=preset["Skeleton"],
        armor=preset["Armor"],
        helm=preset["Helm"],
        weapon=preset["Weapon"],
        ranged=preset["Ranged"],
        magic=preset["Magic"],
    )


def pulse_buttons(buttons: Button, elapsed: float, frequency_hz: float) -> Button:
    if frequency_hz <= 0:
        return buttons
    phase = int(max(0.0, elapsed) * frequency_hz * 2.0)
    return buttons if phase % 2 == 0 else Button(0)


def mirror_fight_state(state: FightState) -> FightState:
    """Representa o mesmo snapshot na composição canônica P1/P2."""

    return FightState(
        round_id=state.round_id,
        tick=state.tick,
        time_left_seconds=state.time_left_seconds,
        time_left_frames=state.time_left_frames,
        left_life=state.left_life,
        left_hit_life=state.left_hit_life,
        right_life=state.right_life,
        right_hit_life=state.right_hit_life,
        time_passed_frames=state.time_passed_frames,
        time_total_frames=state.time_total_frames,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Diagnóstico do transporte UDP do Shadow Fight 2 LAN")
    parser.add_argument("mode", choices=[mode.value for mode in PeerMode] + ["relay"])
    parser.add_argument("--token", required=True, help="Token compartilhado da sala")
    parser.add_argument("--peer", help="IP do host; obrigatório para client")
    parser.add_argument("--port", type=int, default=24190)
    parser.add_argument("--second-port", type=int, help="segunda entrada do relay para emuladores com NAT identico")
    parser.add_argument("--name", default="desktop-diagnostic")
    parser.add_argument(
        "--mirror-profile",
        action="store_true",
        help="devolve o PROFILE recebido e envia READY para testar o handshake Android",
    )
    parser.add_argument(
        "--mirror-equipment",
        choices=sorted(EQUIPMENT_PRESETS),
        help="troca o conjunto do perfil espelhado para um teste visual deterministico",
    )
    parser.add_argument(
        "--send-input",
        nargs=3,
        metavar=("HORIZONTAL", "VERTICAL", "BUTTONS"),
        help="envia um estado uma vez apos conectar; BUTTONS aceita decimal ou 0x hexadecimal",
    )
    parser.add_argument("--input-duration", type=float, default=0.0, help="repete --send-input por N segundos")
    parser.add_argument("--input-hz", type=float, default=30.0, help="frequencia do envio repetido (padrao: 30)")
    parser.add_argument(
        "--input-pulse-hz",
        type=float,
        default=0.0,
        help="alterna os botoes de --send-input entre pressionado e solto; util para golpes repetidos",
    )
    parser.add_argument(
        "--auto-start-delay-ms",
        type=int,
        default=0,
        help=(
            "coordena START com este atraso; no relay, inicia quando os dois clientes "
            "enviam FIGHT_READY (use 2000 no diagnostico Android)"
        ),
    )
    parser.add_argument(
        "--mirror-state",
        action="store_true",
        help="devolve STATE na composicao canonica para testar a comparacao com um Android",
    )
    args = parser.parse_args()

    if args.mirror_equipment and not args.mirror_profile:
        parser.error("--mirror-equipment exige --mirror-profile")
    if args.auto_start_delay_ms and not 500 <= args.auto_start_delay_ms <= 10_000:
        parser.error("--auto-start-delay-ms deve ficar entre 500 e 10000")
    if args.mirror_state and args.mode != PeerMode.HOST.value:
        parser.error("--mirror-state exige o modo host")
    if args.input_pulse_hz:
        if not args.send_input or args.input_duration <= 0:
            parser.error("--input-pulse-hz exige --send-input e --input-duration positivo")
        if not 0 < args.input_pulse_hz <= 10:
            parser.error("--input-pulse-hz deve ficar entre 0 e 10")

    if args.mode == "relay":
        if args.send_input:
            parser.error("--send-input nao se aplica ao modo relay")
        relay = LanRoomRelay(
            args.token,
            port=args.port,
            second_port=args.second_port,
            name=args.name,
            fight_start_delay_ms=args.auto_start_delay_ms or None,
        )
        listeners = ", ".join(f"{host}:{port}" for host, port in relay.local_addresses)
        print(f"SF2 LAN relay ouvindo em {listeners}")
        try:
            while True:
                for event in relay.poll(0.25):
                    if event.packet is not None and event.packet.kind in (
                        PacketType.INPUT,
                        PacketType.STATE,
                        PacketType.FIGHT_AUTHORITY,
                    ):
                        continue
                    print(f"{event.name}: {event.address} {event.detail}".rstrip())
                for event in relay.tick():
                    print(f"{event.name}: {event.address}")
        except KeyboardInterrupt:
            return 0
        finally:
            relay.close()

    peer = LanPeer(PeerMode(args.mode), args.token, port=args.port, server_host=args.peer, device_name=args.name)
    print(f"SF2 LAN {args.mode} ouvindo em {peer.local_address[0]}:{peer.local_address[1]}")
    peer.start()
    input_sent = False
    input_started_at: float | None = None
    next_input_at = 0.0
    last_received_input: InputState | None = None
    last_state_print_at = 0.0
    mirrored_profile = False
    fight_start: FightStart | None = None
    try:
        while True:
            for event in peer.poll(0.01 if args.input_duration > 0 else 0.25):
                if event.name == "connected":
                    mirrored_profile = False
                    fight_start = None
                    last_received_input = None
                suffix = ""
                if event.input_state is not None:
                    if event.input_state == last_received_input:
                        continue
                    last_received_input = event.input_state
                    suffix = (
                        f" horizontal={event.input_state.horizontal:.3f}"
                        f" vertical={event.input_state.vertical:.3f}"
                        f" buttons=0x{int(event.input_state.buttons):02x}"
                    )
                elif event.fighter_profile is not None:
                    profile = event.fighter_profile
                    suffix = (
                        f" level={profile.level} avatar={profile.avatar}"
                        f" weapon={profile.weapon} armor={profile.armor}"
                    )
                elif event.fight_state is not None:
                    state = event.fight_state
                    if args.mirror_state:
                        peer.send_state(mirror_fight_state(state))
                    state_now = time.monotonic()
                    if state_now - last_state_print_at < 0.5:
                        continue
                    last_state_print_at = state_now
                    suffix = (
                        f" round={state.round_id} tick={state.tick}"
                        f" timer={state.time_left_seconds:.0f}s/{state.time_left_frames:.0f}f"
                        f" life=({state.left_life:.3f},{state.right_life:.3f})"
                        f" hit=({state.left_hit_life:.3f},{state.right_hit_life:.3f})"
                        f" elapsed={state.time_passed_frames:.0f}/{state.time_total_frames:.0f}f"
                    )
                print(f"{event.name}: {event.address} {event.detail}{suffix}".rstrip())
                if (
                    args.mirror_profile
                    and event.fighter_profile is not None
                    and not mirrored_profile
                ):
                    mirrored = event.fighter_profile
                    if args.mirror_equipment:
                        mirrored = profile_with_equipment(mirrored, args.mirror_equipment)
                    peer.send_profile(mirrored)
                    peer.send_ready()
                    mirrored_profile = True
                    detail = f" com conjunto {args.mirror_equipment}" if args.mirror_equipment else ""
                    print(f"perfil espelhado{detail} e READY enviados")
                if event.name == "fight-ready" and args.auto_start_delay_ms:
                    if fight_start is None:
                        round_id = int(time.time_ns() & 0x7FFFFFFF) or 1
                        fight_start = FightStart(round_id, args.auto_start_delay_ms)
                    sequence = peer.send_start(fight_start)
                    print(
                        f"START enviado: seq={sequence} round={fight_start.round_id} "
                        f"delay={fight_start.delay_ms}ms"
                    )
                elif event.name == "start-ack" and fight_start is not None:
                    if event.round_id == fight_start.round_id:
                        print(f"START_ACK confirmado: round={event.round_id}")
            now = time.monotonic()
            should_send_once = not input_sent and args.input_duration <= 0
            should_repeat = args.input_duration > 0 and now >= next_input_at
            if peer.connected and args.send_input and (should_send_once or should_repeat):
                horizontal = float(args.send_input[0])
                vertical = float(args.send_input[1])
                buttons = Button(int(args.send_input[2], 0))
                if input_started_at is None:
                    if args.input_duration < 0 or args.input_hz <= 0:
                        parser.error("--input-duration e --input-hz devem ser positivos")
                    input_started_at = now
                elapsed = now - input_started_at
                if args.input_duration > 0 and now - input_started_at >= args.input_duration:
                    sequence = peer.send_input(InputState())
                    print(f"input neutralizado: seq={sequence}")
                    input_sent = True
                    args.send_input = None
                    continue
                if args.input_pulse_hz > 0:
                    buttons = pulse_buttons(buttons, elapsed, args.input_pulse_hz)
                sequence = peer.send_input(InputState(horizontal, vertical, buttons))
                if not input_sent:
                    print(
                        f"input enviado: seq={sequence} horizontal={horizontal} "
                        f"vertical={vertical} buttons=0x{int(buttons):02x}"
                    )
                input_sent = True
                next_input_at = now + 1.0 / args.input_hz
            for event in peer.tick():
                print(f"{event.name}: {event.address}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        return 0
    finally:
        peer.close()


if __name__ == "__main__":
    raise SystemExit(main())
