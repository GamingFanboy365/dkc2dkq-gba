# Tools

Host tools for the port. Build them with the top-level CMake project:

    cmake -S . -B build -DDKC2_ROM=baserom.sfc -DASAR=/path/to/asar
    cmake --build build -j4

- `snesrun.c`: runs the original ROM on LakeSnes (the reference), headless:
  pictures, memory dumps, a per-frame RAM hash at the frame's first WAI, and
  the CPU trace the recompiler uses. Options are environment variables; see
  the top of the file.
- `strip_rom_data.py`: the one-time conversion that took the game's data out
  of the disassembly (incbins from `baserom.sfc` instead).
- `recomp/`: the static recompiler. `asm_probe.py` asks asar where every
  instruction and label of the disassembly is; `recomp.py` decodes them from
  the ROM, works out the M/X widths, corrects the disassembly where the trace
  shows it decoded bytes at the wrong width, and writes C (one function per
  routine) for `src/cpu65.h`'s runtime. Its output is built from the ROM and
  never committed.
- `make_cartridge.py`: the GBA cartridge: the engine, then the player's
  ROM (checked by hash), with the header filled in.
- `mgba/gbarun.c`: runs a GBA cartridge headless (pictures, savestates,
  memory dumps); the cartridge's debug prints go to stdout.
- `compare_sync.py`: compares the per-frame WRAM hashes of the reference
  (`snesrun SYNC=`) and the recompiled build (`hostrun SYNC=`).

The recompiled game on the PC is `host/hostrun.c`. A check run:

    FRAMES=3000 SYNC=ref.sync build/snesrun baserom.sfc
    FRAMES=3000 SYNC=host.sync build/hostrun baserom.sfc
    tools/compare_sync.py ref.sync host.sync

`scripts/make_play_script.py` prints the button script of the play session
(`traces/play1.trace` is `snesrun TRACE=` over it, 8000 frames).

`traces/` holds the reference's CPU traces (`snesrun TRACE=`): addresses
and flags only, no game data.
