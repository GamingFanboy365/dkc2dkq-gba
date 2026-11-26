# Tools

Host tools for the port. Build them with the top-level CMake project or by hand:

    gcc -O2 -o snesrun tools/snesrun.c tools/common/tool_common.c third_party/lakesnes/*.c -lpng -lm

- `snesrun.c`: runs the original ROM on LakeSnes (the reference), headless:
  pictures, memory dumps, a per-frame RAM hash at the frame's first WAI, and
  the CPU trace the recompiler uses. Options are environment variables; see
  the top of the file.
- `strip_rom_data.py`: the one-time conversion that took the game's data out
  of the disassembly (incbins from `baserom.sfc` instead).
