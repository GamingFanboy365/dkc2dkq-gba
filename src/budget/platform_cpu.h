// Clock and checks for builds on the port's own hardware model (hostmini,
// the GBA): a frame's budget of CPU cycles, counted roughly (7 master
// cycles per base CPU cycle, 357,368 a frame), after which the NMI is due
// even if the game hasn't waited for it, as on a SNES in a lag frame.
#ifndef PLATFORM_CPU_H
#define PLATFORM_CPU_H

#include <stdint.h>

extern uint32_t g_cycles, g_frame_end;
#define CYC(n) (g_cycles += (n) * 7)
#define EVENT_PENDING() (g_cycles >= g_frame_end)

#ifdef CHECK_ASSUMPTIONS
void assume_failed(uint32_t pc, int m, int x);
#define ASSUME_MX(m, x, pc) do { \
    if (((m) != 2 && cpu.mf != (m)) || ((x) != 2 && cpu.xf != (x))) assume_failed(pc, m, x); } while (0)
#else
#define ASSUME_MX(m, x, pc) ((void)0)
#endif

#endif
