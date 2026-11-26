#!/usr/bin/env python3
"""Replaces the game's data in the disassembly with incbins from the player's ROM.

The disassembly once held every byte of the ROM: graphics, levels and music
as `db` lines. This rewrites each run of plain-number data lines (`db`, `dw`,
`dl`, `dd` with numbers only, MIN_BYTES or more, outside `if` blocks) as
`incbin "baserom.sfc":$START..$END`, keeping every label, so the disassembly
still builds the same ROM but only from the player's own copy.

It asks asar where each run is (by inserting `print`s into a scratch copy),
rewrites the bank files in place, then builds and checks the result's sha256.

  tools/strip_rom_data.py ASAR BASEROM [MIN_BYTES]

Run it once; running it again finds nothing left to strip (or only runs
added since).
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROM_SHA256 = "b79c2bb86f6fc76e1fc61c62fc16d51c664c381e58bc2933be643bbc4d8b610c"
NUM = r"(?:\$[0-9A-Fa-f]+|%[01]+|[0-9]+)"
DATA_RE = re.compile(r"^\s*(db|dw|dl|dd)\s+(" + NUM + r"(?:\s*,\s*" + NUM + r")*)\s*(;.*)?$")
COND_OPEN = re.compile(r"^\s*if\b", re.I)
COND_CLOSE = re.compile(r"^\s*endif\b", re.I)
SIZES = {"db": 1, "dw": 2, "dl": 3, "dd": 4}


def find_runs(lines, min_bytes):
    """Yields (first, last, size) line ranges of plain data outside if blocks."""
    depth = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if COND_OPEN.match(line):
            depth += 1
        elif COND_CLOSE.match(line):
            depth -= 1
        m = DATA_RE.match(line) if depth == 0 else None
        if not m:
            i += 1
            continue
        first, size = i, 0
        while i < len(lines):
            m = DATA_RE.match(lines[i])
            if not m:
                break
            size += SIZES[m.group(1)] * len(m.group(2).split(","))
            i += 1
        if size >= min_bytes:
            yield first, i - 1, size


def snes_to_rom(addr):
    bank, low = addr >> 16, addr & 0xFFFF
    if bank >= 0xC0:
        return addr & 0x3FFFFF
    if 0x80 <= bank < 0xC0 and low >= 0x8000:
        return (bank & 0x3F) << 16 | low
    raise ValueError(f"not a ROM address: {addr:06X}")


def sha256(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main():
    asar, baserom = sys.argv[1], sys.argv[2]
    min_bytes = int(sys.argv[3]) if len(sys.argv) > 3 else 16
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if sha256(baserom) != ROM_SHA256:
        sys.exit("the ROM isn't Donkey Kong Country 2 (USA, Rev 1)")
    banks = sorted(f for f in os.listdir(root) if re.match(r"bank_[0-9A-F]{2}\.asm$", f))
    texts = {b: open(os.path.join(root, b), encoding="utf-8").read().split("\n") for b in banks}
    runs = {b: list(find_runs(texts[b], min_bytes)) for b in banks}

    # Ask asar where every run starts.
    with tempfile.TemporaryDirectory() as tmp:
        for f in os.listdir(root):
            if f.endswith(".asm"):
                shutil.copy(os.path.join(root, f), tmp)
        shutil.copy(baserom, os.path.join(tmp, "baserom.sfc"))
        for b in banks:
            lines = list(texts[b])
            for n, (first, _, _) in reversed(list(enumerate(runs[b]))):
                lines.insert(first, f'print "STRIP {b} {n} ",hex(realbase())')
            open(os.path.join(tmp, b), "w", encoding="utf-8").write("\n".join(lines))
        out = subprocess.run([asar, "all.asm", "probe.sfc"], cwd=tmp, capture_output=True, text=True)
        if out.returncode:
            sys.exit(out.stdout + out.stderr)
        where = {}
        for line in out.stdout.splitlines():
            parts = line.split()
            if len(parts) == 4 and parts[0] == "STRIP":
                where[(parts[1], int(parts[2]))] = int(parts[3], 16)

    total = 0
    for b in banks:
        lines = texts[b]
        for n, (first, last, size) in reversed(list(enumerate(runs[b]))):
            start = snes_to_rom(where[(b, n)])
            indent = re.match(r"^\s*", lines[first]).group(0) or "\t"
            lines[first:last + 1] = [f'{indent}incbin "baserom.sfc":${start:06X}..${start + size:06X}']
            total += size
    for b in banks:
        open(os.path.join(root, b), "w", encoding="utf-8").write("\n".join(texts[b]))
    print(f"replaced {sum(len(r) for r in runs.values())} runs, {total} bytes")

    # Check: the stripped disassembly must build the same ROM.
    with tempfile.TemporaryDirectory() as tmp:
        for f in os.listdir(root):
            if f.endswith(".asm"):
                shutil.copy(os.path.join(root, f), tmp)
        shutil.copy(baserom, os.path.join(tmp, "baserom.sfc"))
        out = subprocess.run([asar, "all.asm", "check.sfc"], cwd=tmp, capture_output=True, text=True)
        if out.returncode:
            sys.exit(out.stdout + out.stderr)
        got = sha256(os.path.join(tmp, "check.sfc"))
    print("rebuild sha256", "matches" if got == ROM_SHA256 else f"DIFFERS: {got}")
    sys.exit(0 if got == ROM_SHA256 else 1)


if __name__ == "__main__":
    main()
