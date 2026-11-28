"""Audit one complete fight's owner-selected actions and remote applications.

This proves ordered delivery/application, not animation phase or world-state sync.
Capture P1 first and P2 second, with queues drained at the end of the sample.
"""
import argparse
import json
import re
from pathlib import Path

EVENT = re.compile(
    r"LAN action (publish|applied): player=([12]) seq=(\d+) "
    r"(?:tick=(\d+)|sourceTick=(\d+) localTick=\d+) name=([\w-]+)"
)
REQUIRED = {"KatanaStepBack", "JumpUp", "KatanaSlash"}


def compare_delivery(first: str, second: str) -> dict:
    streams = []
    for role, text in enumerate((first, second), 1):
        events = {"publish": [], "applied": []}
        if "LAN action queue full" in text:
            raise ValueError("Action queue overflow")
        for match in EVENT.finditer(text):
            kind, player, seq, tick, source_tick, name = match.groups()
            expected_player = role if kind == "publish" else 3 - role
            if int(player) != expected_player:
                raise ValueError("Wrong owner or capture order")
            if int(seq) != len(events[kind]) + 1:
                raise ValueError("Duplicate, gap, or multiple fights in capture")
            events[kind].append((int(seq), int(tick or source_tick), name))
        streams.append(events)
    report = {"passed": True, "players": {}}
    for index in range(2):
        published = streams[index]["publish"]
        applied = streams[1 - index]["applied"]
        missing = sorted(REQUIRED - {e[2] for e in published})
        passed = bool(published) and published == applied and not missing
        report["players"][str(index + 1)] = {
            "passed": passed, "published": len(published), "applied": len(applied),
            "exact_sequence": published == applied, "missing_actions": missing,
            "slash_count": sum(e[2] == "KatanaSlash" for e in published),
        }
        report["passed"] &= passed
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("p1_log", type=Path)
    parser.add_argument("p2_log", type=Path)
    args = parser.parse_args()
    report = compare_delivery(args.p1_log.read_text(encoding="utf-8-sig"),
                              args.p2_log.read_text(encoding="utf-8-sig"))
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
