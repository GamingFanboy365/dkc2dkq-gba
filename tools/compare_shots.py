#!/usr/bin/env python3
"""Compares pictures of the same game frames from two builds.

  compare_shots.py REF.sync REF_DIR TEST_DIR OUT_DIR [--crop X,Y,W,H]

REF_DIR has the reference's shot_FFFFF.png (snesrun SHOTS=, by its frame
number); TEST_DIR has PREFIX_N.png pictures named by the game's wait N
(hostmini's mini_shot_N.png, the GBA's gba_shot_N.png). REF.sync (snesrun
SYNC=) says which reference frame each wait was in. For each test picture
it prints the share of pixels that differ (any channel by more than 8) and
writes OUT_DIR/cmp_N.png: reference, test, difference.

To get the reference's frames for a list of waits:
  compare_shots.py REF.sync --frames N,N,...
"""
import os
import re
import sys

from PIL import Image, ImageChops


def main():
    sync = [int(line.split()[0]) for line in open(sys.argv[1])]
    if sys.argv[2] == "--frames":
        print(",".join(str(sync[int(n)]) for n in sys.argv[3].split(",")))
        return
    ref_dir, test_dir, out_dir = sys.argv[2:5]
    crop = None
    if "--crop" in sys.argv:
        x, y, w, h = map(int, sys.argv[sys.argv.index("--crop") + 1].split(","))
        crop = (x, y, x + w, y + h)
    os.makedirs(out_dir, exist_ok=True)
    worst = 0.0
    for name in sorted(os.listdir(test_dir), key=lambda s: [int(t) for t in re.findall(r"\d+", s)] or [0]):
        m = re.match(r"[a-z_]+_(\d+)\.png$", name)
        if not m or name.startswith("cmp_"):
            continue
        n = int(m.group(1))
        ref_path = os.path.join(ref_dir, f"shot_{sync[n]:05d}.png")
        if not os.path.exists(ref_path):
            continue
        ref = Image.open(ref_path).convert("RGB")
        test = Image.open(os.path.join(test_dir, name)).convert("RGB")
        if crop:
            ref = ref.crop(crop)
        if ref.size != test.size:
            print(f"wait {n}: sizes differ, {ref.size} and {test.size}")
            continue
        diff = ImageChops.difference(ref, test).point(lambda v: 255 if v > 8 else 0).convert("L")
        bad = sum(1 for v in diff.getdata() if v)
        share = bad / (ref.size[0] * ref.size[1])
        worst = max(worst, share)
        print(f"wait {n} (reference frame {sync[n]}): {100 * share:.2f}% of pixels differ")
        sheet = Image.new("RGB", (ref.size[0] * 3, ref.size[1]))
        sheet.paste(ref, (0, 0))
        sheet.paste(test, (ref.size[0], 0))
        sheet.paste(diff.convert("RGB"), (ref.size[0] * 2, 0))
        sheet.save(os.path.join(out_dir, f"cmp_{n}.png"))
    print(f"worst: {100 * worst:.2f}%")


if __name__ == "__main__":
    main()
