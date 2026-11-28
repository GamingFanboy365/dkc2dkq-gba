#!/usr/bin/env python3
"""Sums the cartridge's own PC samples ('S pc' lines, from a build with
-DDKC2_SAMPLE_PROFILE=FRAME) by function, from the ELF's symbols.

  sample_report.py GBARUN_OUTPUT dkc2.elf [TOP]
"""
import bisect
import collections
import subprocess
import sys

pcs = [int(l.split()[1], 16) for l in open(sys.argv[1]) if l.startswith("S ")]
syms = []
for line in subprocess.run(["llvm-nm", "-n", "-S", sys.argv[2]], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 4 and p[2] in "tTwW":
        syms.append((int(p[0], 16) & ~1, int(p[1], 16), p[3]))
syms.sort()
starts = [s[0] for s in syms]
by_fn = collections.Counter()
for pc in pcs:
    k = bisect.bisect_right(starts, pc & ~1) - 1
    if k >= 0 and pc < syms[k][0] + max(syms[k][1], 1):
        by_fn[syms[k][2]] += 1
    elif pc < 0x4000:
        by_fn["(BIOS)"] += 1
    else:
        by_fn["(other)"] += 1
groups = collections.Counter()
for name, n in by_fn.items():
    g = "game (R_*)" if name.startswith("R_") else "thunks" if name.startswith("__") else "other"
    groups[g] += n
top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
print(f"{len(pcs)} samples: " + ", ".join(f"{g} {100 * n / len(pcs):.1f}%" for g, n in groups.most_common()))
for name, n in by_fn.most_common(top):
    print(f"{100 * n / len(pcs):6.2f}%  {name}")
