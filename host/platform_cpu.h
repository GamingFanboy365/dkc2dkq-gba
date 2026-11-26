// The host build's clock and checks for the recompiled code (src/cpu65.h).
#ifndef PLATFORM_CPU_H
#define PLATFORM_CPU_H

#include <stdint.h>
#include <stdbool.h>

// A rough clock: 7 master cycles per CPU cycle of the instruction's base
// count. The hardware (LakeSnes's PPU, DMA, APU) is caught up to it before
// every I/O access and when polled.
extern uint64_t g_cpu_cycles, g_event_at;
bool host_poll(void);
#define CYC(n) (g_cpu_cycles += (n) * 7)
#define EVENT_PENDING() (g_cpu_cycles >= g_event_at && host_poll())

void assume_failed(uint32_t pc, int m, int x);
#define ASSUME_MX(m, x, pc) do { \
    if (((m) != 2 && cpu.mf != (m)) || ((x) != 2 && cpu.xf != (x))) assume_failed(pc, m, x); } while (0)

#endif
