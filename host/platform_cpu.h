// The host build's clock and checks for the recompiled code (src/cpu65.h).
#ifndef PLATFORM_CPU_H
#define PLATFORM_CPU_H

#include <stdint.h>
#include <stdbool.h>

// The memory map: LakeSnes's, through hostrun.c.
uint8_t rd8(uint32_t addr);
void wr8(uint32_t addr, uint8_t value);
static inline uint8_t rd8_b0(uint16_t a) { return rd8(a); }
static inline void wr8_b0(uint16_t a, uint8_t v) { wr8(a, v); }

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
