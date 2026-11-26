#!/usr/bin/env python3
"""Compares two SYNC files (snesrun's and hostrun's): the hash of WRAM at
every WAI, less the counter of all NMIs.

  compare_sync.py REFERENCE TEST

The game's logic runs a frame per NMI and then waits (WAI). Lag frames, and
the host's rough clock, move waits from frame to frame and add waits where
nothing but the NMI counter changed, so the comparison is of the sequence of
states, each run of equal hashes counted once. It prints how many states
match, and where the first difference is.
"""
import sys


def load(path):
    rows = []
    for line in open(path):
        f, h, pc = line.split()
        if not rows or rows[-1][1] != h:
            rows.append((int(f), h, pc))
    return rows


def main():
    ref, test = load(sys.argv[1]), load(sys.argv[2])
    n = min(len(ref), len(test))
    k = next((i for i in range(n) if ref[i][1] != test[i][1]), None)
    if k is None:
        print(f"all {n} states match (reference {len(ref)}, test {len(test)})")
        if n:
            print(f"last: reference frame {ref[n - 1][0]}, test frame {test[n - 1][0]}")
    else:
        print(f"the first {k} states match; state {k}: reference frame {ref[k][0]} {ref[k][1]} at {ref[k][2]}, "
              f"test frame {test[k][0]} {test[k][1]} at {test[k][2]}")
    sys.exit(0 if k is None else 1)


if __name__ == "__main__":
    main()
