"""Measure action-frame progression separately from simulation ticks.

One fight per capture. Reports rates, NOT synchronization success: local ticks
and wall clocks are not a shared timeline. No inference about damage or physics.
"""
import argparse
import json
import re
from pathlib import Path

PHASE = re.compile(
    r"LAN action phase: role=([12]) player=([12]) seq=(\d+) tick=(\d+) "
    r"frame=(-?\d+) name=([\w-]+)\."
)
MOVES = {"KatanaSlash", "KatanaStepBack", "JumpUp"}


def summarize_phase(text: str) -> list[dict]:
    groups = {}
    role_seen = None
    for match in PHASE.finditer(text):
        role, player, seq, tick, frame = map(int, match.groups()[:5])
        name = match[6]
        if role_seen is not None and role != role_seen:
            raise ValueError("Capture contains multiple roles")
        role_seen = role
        if name not in MOVES:
            continue
        samples = groups.setdefault((role, player, seq, name), [])
        if samples and tick <= samples[-1][0]:
            raise ValueError("Non-increasing ticks: duplicate capture or multiple fights")
        samples.append((tick, frame))
    result = []
    for (role, player, seq, name), samples in groups.items():
        tick_span = samples[-1][0] - samples[0][0]
        frame_span = samples[-1][1] - samples[0][1]
        result.append(dict(role=role, player=player, sequence=seq, name=name,
                           samples=len(samples), tick_span=tick_span, frame_span=frame_span,
                           frames_per_tick=round(frame_span / tick_span, 4) if tick_span else None,
                           frame_reversed=any(b[1] < a[1] for a, b in zip(samples, samples[1:]))))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    result = summarize_phase(args.log.read_text(encoding="utf-8-sig"))
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result else 1)


if __name__ == "__main__":
    main()
