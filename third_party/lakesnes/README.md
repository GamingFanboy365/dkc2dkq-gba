LakeSnes, a SNES emulator in C by angelo_wf (MIT licence, LICENSE.txt), from
https://github.com/angelo-wf/LakeSnes at commit 9db90b86e46a377609305e298dd92d71cd1d4c8a.

It is the port's reference: `tools/snesrun.c` runs the original ROM on it, and
the host build of the recompiled game uses its PPU, DMA and APU.

Local changes (search for `DKC2PORT`):
- `cpu.c`: an optional hook called before each opcode (`cpu_traceHook`), for
  the trace that tells the recompiler the M/X flags and indirect jump targets.
