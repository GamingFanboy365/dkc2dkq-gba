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

- **The game's code, recompiled**: 79,844 instructions, as 2,649 C
  functions (the disassembly's routines, merged where they pass control to
  each other by branches and jumps). The disassembly says where the code is,
  the ROM what it is; the M/X register widths come from a flow analysis and
  traces of the original (`traces/`). The traces also correct the
  disassembly where it decoded bytes at the wrong width (five spots in bank
  B5).
- **Checked against an emulator**: WRAM after every frame of the game's
  logic matches the original running on LakeSnes, on LakeSnes's hardware
  (`hostrun`) and on the port's own hardware model (`hostmini`), over two
  sessions: power on to the title screen (2805 game frames), and a scripted
  play session (6193 game frames: file select, the world map, Pirate Panic
  with runs, jumps, rolls, team-ups, swaps, pauses, deaths, the cabin, the
  map again and back in). The GBA's states match too, as far as checked.
- **The picture**: `gba/render.c` draws the game with the GBA's video
  hardware (see its top comment). The SNES's 256x224 picture on the GBA's
  240x160: on the title, menus and maps all 224 lines, squeezed (a GBA line
  shows every 1.4th SNES line); in the levels 184 of them, about as wide for
  their height as a TV shows the SNES's, the window following the Kong
  (`gba/main.c`). Columns 8-247 either way. Sprites are made each frame of
  the same lines as the backgrounds (no seams where pieces meet), less the
  pixels BG1's and BG2's priority tiles cover. Modes 1, 3 (8bpp), 5 (its
  lines under Nintendo Presents) and 7 (the GBA's affine BG); the main and
  sub screens, color math (adding), the color window, and what HDMA changes
  line by line (scroll, layers, color math, a couple of colors, the mode)
  through HBlank DMA and an HBlank interrupt. Not right yet: BG2's priority
  tiles in front of BG1 (the lights on the world map's sign), color math
  that adds the sub screen to two layers at once (the mist over the
  island), subtracting, HDMA changing Mode 7's matrix.
- **Widths no trace saw**: where no trace has run the code, the
  recompiler takes instruction boundaries from the disassembly, which
  decodes some bytes at the wrong register width. Where the width analysis
  proves another width (all of an immediate's predecessors agree), it
  re-decodes from the ROM: one spot in the whole game ($B5F74C, which the
  native version of its routine also does right). Where the analysis can't
  tell, the generated code tests the width at run time, but the
  instruction boundaries are still the disassembly's: a trace that runs
  more of the game would settle those too.
- **Not yet**: sound (the sound CPU only answers handshakes); saves; only
  the first level is verified (code the traces haven't seen run may have
  entries or register widths the recompiler doesn't know).

## Speed

The game's frame is the SNES's sixtieth of a second, so the frame rate is
its speed: to play at full speed a frame must take no more than one of the
GBA's (280,896 cycles). It's paced to the GBA's 60 frames a second (never
faster), and when it's late a frame's picture is skipped, never two in a
row (`gba/main.c`): the game runs as fast as its logic allows, the picture
at half that or more. The cartridge's timing lines say how many VBlanks
each 60 of the game's frames took (60: full speed); Pirate Panic takes
about 75, 80% of full speed (84-94 in its busiest stretch).

GBA cycles per game frame in Pirate Panic (a GBA frame is 280,896):

| | Logic | Drawing | Together |
|---|---|---|---|
| First build | 1,530,000 | 920,000 | 2,450,000 (8.7 frames) |
| Before the renderer's rework | 555,000 | 775,000 | 1,330,000 (4.7 frames) |
| Before HDMA by channel | 293,000 | 276,000 | 569,000 (2.0 frames) |
| HDMA by channel | 293,000 | 174,000 | 467,000 (1.7 frames) |
| Now | 245,000 | 174,000 | 419,000 (1.5 frames) |

(Measured over game frames 2340-2399 of the play session, in the level,
with every frame drawn; an earlier "315,000 for drawing" was a quieter
stretch.)

What did it, measured with gbarun's sampling profiler (`PROFILE=`,
`tools/profile_report.py`): the memory map inline, DMA to VRAM/OAM/CGRAM as
copy loops, calls that return normally kept native, routines merged into
bigger functions, a page index for the dispatcher, no cycle counting on the
GBA; in the renderer, map cells and tile slots rewritten only when what they
show changes, VRAM's dirty bits, hot loops as ARM in IWRAM, a 32-column
map's row drawn once whatever each line's scroll, HDMA run on a copy of
its state in IWRAM, and, when its channels only scroll a layer or set the
fixed color (the levels' parallax and gradients), run channel by channel
straight into the renderer's line arrays (`hw_hdma_frame_fast`: 135,000
cycles a frame down to 20,000), the recompiled code as Thumb (half the ROM fetches), and native C for
the hottest routines (`src/native.c`: the objects' sprites into OAM, the
levels' parallax tables, the sort of the objects by drawing order, the
levels' tiled objects' sprites, the decompressor, which loading and the
levels' transitions spend their time in, and the unused OAM slots' reset).
The dispatcher runs from IWRAM with a cache of its recent targets.
In the generated code, D and DB are locals, and an access whose address is
constant when they are has a copy for the values the trace saw there;
indexed accesses test their index's range once and go straight to WRAM or
ROM, block moves copy by pointers, and a function tests its first entry
before searching the others. A call, a return or an exit stores only the
registers the function may have changed since it loaded them (a forward
analysis over its gotos): a third of the stores it made before.
What's left: the game's own logic (the recompiled code:
about 60%: executing it, not its memory: it makes only ~5,000 WRAM
accesses a frame, half in the direct page and stack), the map rows, and
HDMA on screens whose channels do more (Crocodile Isle's map, whose
palette, layers and scroll change every line: still line by line). IWRAM (32 KB) and EWRAM (256 KB, half of
it the SNES's WRAM) are nearly full; a guard word past IWRAM's data,
checked every frame, reports the stack reaching it.

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

    # The play session (the script's frames are the game's own, so every
    # build sees the same buttons):
    export KEYS=$(tools/scripts/make_play_script.py 1 6000) FRAMES=8000
    SYNC=ref.sync build/snesrun baserom.sfc
    SYNC=mini.sync build/hostmini baserom.sfc
    tools/compare_sync.py ref.sync mini.sync

    # The GBA cartridge (build-gba/dkc2.gba):
    cmake -S . -B build-gba -DCMAKE_TOOLCHAIN_FILE=cmake/gba-toolchain.cmake \
          -DDKC2_ROM=$PWD/baserom.sfc -DASAR=/path/to/asar
    cmake --build build-gba -j4
    gbarun -q -n 20000 build-gba/dkc2.gba     # prints cycles per frame

With `-DDKC2_SYNC_LOG=ON` the cartridge prints a WRAM hash per frame
(`sync FRAME HASH PC`), for `compare_sync.py`. With
`-DDKC2_SAMPLE_PROFILE=FRAME` it samples its own PC from a timer interrupt
over 600 game frames from FRAME, evenly in time (gbarun's `PROFILE=` samples
when mGBA handles events, which over-counts the interrupt handlers):

    gbarun -q -n 9000 build-gba/dkc2.gba > out.txt
    tools/sample_report.py out.txt build-gba/dkc2.elf `tools/mgba/README.md` builds
gbarun.

## Controls

See `docs/controls.md`: the 2004 GBA version's layout (A jump, B
run/attack/carry, R team-up, L or Select swap), mapped onto the SNES
joypad in `gba/main.c`.
