# Donkey Kong Country 2 on the Game Boy Advance

This repository is a disassembly of Donkey Kong Country 2 (SNES, USA, Rev 1)
and, on top of it, the start of a port to the GBA. The port follows the
approach of the Super Metroid GBA port's guide: run the game's own logic as
native code, and replace only the platform (video, sound, input, saves).
DKC2 has no C decompilation, so the port recompiles the disassembly's 65816
code to C mechanically (`tools/recomp`).

Nothing here contains the game: the disassembly incbins its data from the
player's ROM, the recompiled C is generated from that ROM at build time, and
the cartridge is built with it. Never commit or share a ROM, the generated
code or a cartridge.

## Where it stands

- **The game's code, recompiled**: 79,838 instructions, 6,379 routines,
  one C function each. The disassembly says where the code is, the ROM what
  it is; the M/X register widths come from a flow analysis and a trace of
  the original (`traces/`). The trace also corrects the disassembly where it
  decoded bytes at the wrong width (`$B5A262`).
- **Checked against an emulator**: from power on through the Rareware and
  Nintendo logos to the title screen (3000 frames), WRAM after every frame
  matches the original running on LakeSnes, on LakeSnes's hardware
  (`hostrun`) and on the port's own hardware model (`hostmini`), whose
  VRAM and CGRAM also match at the title.
- **On the GBA**: the same code and model run on the GBA (mGBA), and the
  states it reached (1622, to the title) match the reference too.
- **Not yet**: nothing is drawn or played on the GBA (the PPU's state is
  recorded, not rendered; the sound CPU only answers handshakes). Nothing
  past the title is verified: the traces cover the boot and the title only.

## Speed, before any optimization

GBA cycles per SNES frame (a GBA frame is 280,896), everything in Thumb in
ROM, every memory access a function call:

| Screen | Cycles per frame |
|---|---|
| Logos | 200,000–300,000 |
| Title | 620,000 (2.2 frames) |
| The sequence after the title | 1.3–2.6 million (5–9 frames) |
| Boot (clearing VRAM and WRAM a byte at a time) | up to 62 million, once |

The guide's remedies are next: hot code as ARM in IWRAM, direct access to
WRAM for the direct page, the stack and bank $7E, DMA by the GBA's DMA, and
dropping the cycle count the PC builds need.

## Building and checking

You need clang/lld (18), cmake, Python 3, libpng, asar 1.9, and your ROM.

    # The disassembly (byte-exact check):
    cp YOUR_ROM.sfc baserom.sfc && make

    # Host tools and PC builds:
    cmake -S . -B build -DDKC2_ROM=$PWD/baserom.sfc -DASAR=/path/to/asar
    cmake --build build -j4
    FRAMES=3000 SYNC=ref.sync build/snesrun baserom.sfc     # the reference
    FRAMES=3000 SYNC=mini.sync build/hostmini baserom.sfc   # the port's model
    tools/compare_sync.py ref.sync mini.sync

    # The GBA cartridge (build-gba/dkc2.gba):
    cmake -S . -B build-gba -DCMAKE_TOOLCHAIN_FILE=cmake/gba-toolchain.cmake \
          -DDKC2_ROM=$PWD/baserom.sfc -DASAR=/path/to/asar
    cmake --build build-gba -j4
    gbarun -q -n 20000 build-gba/dkc2.gba     # prints cycles per frame

With `-DDKC2_SYNC_LOG=ON` the cartridge prints a WRAM hash per frame
(`sync FRAME HASH PC`), for `compare_sync.py`. `tools/mgba/README.md` builds
gbarun.

## Controls

See `docs/controls.md`: the 2004 GBA version's layout (A jump, B
run/attack/carry, R team-up, L or Select swap), mapped onto the SNES
joypad in `gba/main.c`.
