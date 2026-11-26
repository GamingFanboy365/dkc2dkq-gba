#!/usr/bin/env python3
"""Prints a KEYS script (see tools/common/tool_common.h) that starts a new
game, enters the first level and plays it with a fixed pseudo-random mix of
moves: runs, jumps, rolls, crouches, team-ups, swaps, pauses.

  make_play_script.py [SEED [GAME_FRAMES]]

The same seed always gives the same script, so a trace made with it can be
made again.
"""
import random
import sys

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
length = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
rng = random.Random(seed)

# Title -> file select -> player select -> world map -> level.
parts = ["860:-"] + ["5:Start", "120:-"] * 4 + ["5:Start", "200:-", "5:B", "200:-", "5:B", "200:-", "5:B", "320:-"]
moves = [
    (30, lambda n: [f"{n}:Right+Y"]),                         # run
    (14, lambda n: [f"{n}:Right+Y", "18:Right+Y+B"]),          # running jump
    (8, lambda n: [f"{n}:Right", "4:Right+Y", "20:Right"]),    # roll
    (6, lambda n: [f"{n}:Left+Y"]),
    (5, lambda n: ["12:B", f"{n}:-"]),                        # jump in place
    (4, lambda n: [f"{n}:Down"]),
    (4, lambda n: ["4:A", f"{n}:-", "4:Y", "30:-"]),           # team-up, throw
    (3, lambda n: ["4:Select", f"{n}:-"]),                     # swap
    (3, lambda n: [f"{n}:Up"]),
    (2, lambda n: ["4:Start", "40:-", "4:Start", "20:-"]),     # pause
    (3, lambda n: [f"{n}:Right+Y+B"]),
    (2, lambda n: ["4:X", f"{n}:-"]),
    (2, lambda n: [f"{n}:L"]),
    (2, lambda n: [f"{n}:R"]),
]
weights = [w for w, _ in moves]
total = sum(int(p.split(":")[0]) for p in parts)
while total < length:
    _, make = rng.choices(moves, weights)[0]
    seq = make(rng.randint(10, 70))
    parts += seq
    total += sum(int(p.split(":")[0]) for p in seq)
print(",".join(parts))
