from __future__ import annotations

import argparse
import time

from .peer import LanPeer, PeerMode


def main() -> int:
    parser = argparse.ArgumentParser(description="Diagnóstico do transporte UDP do Shadow Fight 2 LAN")
    parser.add_argument("mode", choices=[mode.value for mode in PeerMode])
    parser.add_argument("--token", required=True, help="Token compartilhado da sala")
    parser.add_argument("--peer", help="IP do host; obrigatório para client")
    parser.add_argument("--port", type=int, default=24190)
    parser.add_argument("--name", default="desktop-diagnostic")
    args = parser.parse_args()

    peer = LanPeer(PeerMode(args.mode), args.token, port=args.port, server_host=args.peer, device_name=args.name)
    print(f"SF2 LAN {args.mode} ouvindo em {peer.local_address[0]}:{peer.local_address[1]}")
    peer.start()
    try:
        while True:
            for event in peer.poll(0.25):
                print(f"{event.name}: {event.address} {event.detail}".rstrip())
            for event in peer.tick():
                print(f"{event.name}: {event.address}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        return 0
    finally:
        peer.close()


if __name__ == "__main__":
    raise SystemExit(main())
