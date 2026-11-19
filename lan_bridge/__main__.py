from __future__ import annotations

import argparse
import time

from .peer import LanPeer, PeerMode
from .protocol import Button, InputState


def main() -> int:
    parser = argparse.ArgumentParser(description="Diagnóstico do transporte UDP do Shadow Fight 2 LAN")
    parser.add_argument("mode", choices=[mode.value for mode in PeerMode])
    parser.add_argument("--token", required=True, help="Token compartilhado da sala")
    parser.add_argument("--peer", help="IP do host; obrigatório para client")
    parser.add_argument("--port", type=int, default=24190)
    parser.add_argument("--name", default="desktop-diagnostic")
    parser.add_argument(
        "--send-input",
        nargs=3,
        metavar=("HORIZONTAL", "VERTICAL", "BUTTONS"),
        help="envia um estado uma vez apos conectar; BUTTONS aceita decimal ou 0x hexadecimal",
    )
    parser.add_argument("--input-duration", type=float, default=0.0, help="repete --send-input por N segundos")
    parser.add_argument("--input-hz", type=float, default=30.0, help="frequencia do envio repetido (padrao: 30)")
    args = parser.parse_args()

    peer = LanPeer(PeerMode(args.mode), args.token, port=args.port, server_host=args.peer, device_name=args.name)
    print(f"SF2 LAN {args.mode} ouvindo em {peer.local_address[0]}:{peer.local_address[1]}")
    peer.start()
    input_sent = False
    input_started_at: float | None = None
    next_input_at = 0.0
    try:
        while True:
            for event in peer.poll(0.01 if args.input_duration > 0 else 0.25):
                print(f"{event.name}: {event.address} {event.detail}".rstrip())
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
                if args.input_duration > 0 and now - input_started_at >= args.input_duration:
                    sequence = peer.send_input(InputState())
                    print(f"input neutralizado: seq={sequence}")
                    input_sent = True
                    args.send_input = None
                    continue
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
