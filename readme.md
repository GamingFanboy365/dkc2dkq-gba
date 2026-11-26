# Donkey Kong Country 2 on the Game Boy Advance

A port of Donkey Kong Country 2: Diddy's Kong Quest (SNES, USA, Rev 1) to the
Game Boy Advance, built on a disassembly of the game. The game's own 65816
code is recompiled to C (`tools/recomp`) and runs on the GBA's ARM CPU; what
the port replaces is the SNES around it. The picture is drawn with the GBA's
video hardware from the SNES's video memory and registers, in every mode the
game uses (Modes 1, 3, 5 and 7), with the main and sub screens, color math,
the color window and what HDMA changes from line to line. The SNES's
256x224 picture is squeezed onto the GBA's 240x160 on the title, menus and
maps, and in the levels the GBA shows 184 of its lines, following the Kong,
much as the handheld versions of these games frame the action.

Every frame of the game's logic is checked against the original running on
an emulator (LakeSnes, in `third_party/`): the port's WRAM must match the
original's byte for byte, from power on to the title and over a scripted play
session through the file select, the world map and the first level, Pirate
Panic, with its cabin. The GBA build matches it too.

### Where it stands

The game boots, its menus and maps work and Pirate Panic plays, but slowly:
a frame of the level takes about 2.5 of the GBA's, so the game runs at about
24 frames a second instead of 60, a little over a third of its speed. About
half of that time is the game's own logic, half the drawing. There's no
sound yet (the SNES's sound CPU only answers the game's handshakes) and no
saving, and only the first level has been checked against the original;
code no recording has run yet may still need fixing. `docs/PORT.md` has the
details: what's done, the speed and how it was gained, what's left.

### Building and playing: bring your own ROM

This repository holds no game data, and neither the recompiled code nor the
cartridge is ever committed: all three are made from the player's own ROM.
Copy your Donkey Kong Country 2: Diddy's Kong Quest (USA, Rev 1) ROM,
unheadered, sha256
`b79c2bb86f6fc76e1fc61c62fc16d51c664c381e58bc2933be643bbc4d8b610c`, to
`baserom.sfc` here. You need clang and lld (18), CMake, Python 3, libpng and
asar 1.9. Then:

    cmake -S . -B build-gba -DCMAKE_TOOLCHAIN_FILE=cmake/gba-toolchain.cmake \
          -DDKC2_ROM=$PWD/baserom.sfc -DASAR=/path/to/asar
    cmake --build build-gba -j4

builds the cartridge, `build-gba/dkc2.gba`, for any GBA emulator (or a
flash cartridge). It contains your ROM, so keep it to yourself. The controls
are the 2004 GBA version's: A jumps, B runs, attacks and carries, R teams up,
L or Select swaps the Kongs, Start pauses (`docs/controls.md`). The PC tools
that check the port against the original are built the same way without the
toolchain file; `docs/PORT.md` shows how to run the checks.

### The disassembly

The game's data (graphics, levels, music, tables) is `incbin`ed from
`baserom.sfc`, so `make` (with asar 1.9) rebuilds the ROM from the
disassembly, checked against the same hash. `tools/strip_rom_data.py` did the
stripping: it replaced every run of 16 or more bytes of plain-number `db`/`dw`
lines with an incbin of the same bytes, keeping the labels. Never commit a ROM
or anything built from one.

Some conventions of the disassembly, more to come:

#### Choosing label names

Routines that act like game loops get names such as `run_<action>`
Routines that primarily delegate many related tasks get the name `<task>_handler`
Routines that are global trampolines for a local routine get a suffix of `<routine>_global`
Use sublabels whenever possible, nesting deeper is okay but should be used sparingly
Relative labels should be used minimally when a name wouldn't help the context (Like a spinlock)
Strive for descriptive names, ideally under 30 chars but absolutely never exceeding 36
Anything in the global namespace should be specific to reduce chances of name conflict
Underscores only.
Use of `#` should generally be temporary (code actively being documented) or if rarely used when it
helps routines with multiple entries or similar conditions

#### Data naming notes
Data which is local should get a sublabel
Palettes should get the `_palette` suffix
Layer graphics should get the `_tiledata` suffix
Layer tile maps should get the `_tilemap` suffix
Collections of sprite graphics should get the `_graphics` suffix
Single sprite graphics should get the suffix `_frameN` where `N` is optional (used for when multiple frames exist)
When practical, ROM data should get a struct defined in `structs.asm`  This allows for named member access

#### Comment structure
Its best to allow comments to flow around the code to cluster related opcodes.  Try to comment
on what is happening and why it is happening more than how it is happening.  The hard limit for line
length is 120 chars.  Do not exceed this with comments.  If you need a bulk description for some reason
place it in a block before the routine.  Never use the macros as a comment trick.
