"""Compare actual Fighter action transitions from two MuMu logcat captures.

Capture one fight per file. This checks action selection and frame skew, not
physical reconciliation, animation phase, hit detection or combat authority.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ACTION = re.compile(
    r"LAN action selected: role=(\d+) player=(\d+) seq=(\d+) tick=(\d+) "
    r"name=(\S+) animation=(\S+)\."
)
REQUIRED = {"KatanaStepBack", "JumpUp", "KatanaSlash"}


def compare_actions(first: str, second: str, max_frame_skew: int = 8) -> dict:
    if max_frame_skew < 0:
        raise ValueError("max_frame_skew must be nonnegative")
    captures = []
    for text, expected_role in ((first, 1), (second, 2)):
        players = {1: [], 2: []}
        for match in ACTION.finditer(text):
            role, player, seq, tick = map(int, match.groups()[:4])
            if role != expected_role or player not in players:
                raise ValueError("Unexpected role/player; pass P1 capture first, P2 second")
            events = players[player]
            if seq != len(events) + 1:
                raise ValueError("Missing transitions or multiple fights; capture one complete fight")
            events.append((tick, match[5], match[6]))
        captures.append(players)
    report = {"passed": True, "max_allowed_frame_skew": max_frame_skew, "players": {}}
    for player in (1, 2):
        left, right = captures[0][player], captures[1][player]
        same = bool(left) and [x[1:] for x in left] == [x[1:] for x in right]
        missing = sorted(REQUIRED - {x[1] for x in left} | REQUIRED - {x[1] for x in right})
        skew = max((abs(a[0] - b[0]) for a, b in zip(left, right)), default=None)
        passed = same and not missing and skew is not None and skew <= max_frame_skew
        report["players"][str(player)] = {
            "passed": passed, "same_sequence": same,
            "transition_counts": [len(left), len(right)],
            "missing_required_actions": missing, "max_frame_skew": skew,
            "actions": [x[1] for x in left],
        }
        report["passed"] &= passed
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("p1_log", type=Path)
    parser.add_argument("p2_log", type=Path)
    parser.add_argument("--max-frame-skew", type=int, default=8)
    args = parser.parse_args()
    report = compare_actions(
        args.p1_log.read_text(encoding="utf-8-sig"),
        args.p2_log.read_text(encoding="utf-8-sig"), args.max_frame_skew,
    )
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
