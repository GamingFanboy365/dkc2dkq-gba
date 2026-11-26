#!/usr/bin/env python3
"""Sums gbarun's PROFILE samples by function, from the ELF's symbols.

  profile_report.py PROFILE_FILE dkc2.elf [TOP]
"""
import bisect
import collections
import subprocess
import sys

samples = [tuple(int(x, 16 if i == 0 else 10) for i, x in enumerate(l.split())) for l in open(sys.argv[1])]
syms = []
for line in subprocess.run(["llvm-nm", "-n", "-S", sys.argv[2]], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 4 and p[2] in "tTwW":
        syms.append((int(p[0], 16) & ~1, int(p[1], 16), p[3]))
syms.sort()
starts = [s[0] for s in syms]
by_fn = collections.Counter()
total = 0
for pc, n in samples:
    total += n
    k = bisect.bisect_right(starts, pc & ~1) - 1
    if k >= 0 and pc < syms[k][0] + max(syms[k][1], 1):
        by_fn[syms[k][2]] += n
    elif 0x03000000 <= pc < 0x03008000:
        by_fn["(IWRAM)"] += n
    elif pc < 0x4000:
        by_fn["(BIOS)"] += n
    else:
        by_fn["(other)"] += n
top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
print(f"{total} samples")
for name, n in by_fn.most_common(top):
    print(f"{100 * n / total:6.2f}%  {name}")
