// The SNES's memory map for the recompiled code, on the port's hardware
// model: WRAM and ROM inline, everything else (I/O, SRAM, open bus) through
// snes_hw.c.
#ifndef SNES_MEM_H
#define SNES_MEM_H

#include <stdint.h>

extern uint8_t g_wram[0x20000];   // 4-byte aligned (snes_hw.c)
#define WRAM_DIRECT 1                  // cpu65.h may access g_wram itself
extern const uint8_t *g_rom;          // 4 MB, HiROM
uint8_t rd8_slow(uint32_t addr);
void wr8_slow(uint32_t addr, uint8_t value);

static inline __attribute__((always_inline)) uint8_t rd8(uint32_t a) {
  uint32_t bank = a >> 16, lo = a & 0xffff;
  if ((bank & 0xfe) == 0x7e) return g_wram[a & 0x1ffff];
  if (!(bank & 0x40)) {                                  // 00-3F, 80-BF
    if (lo < 0x2000) return g_wram[lo];
    if (lo >= 0x8000) return g_rom[(bank & 0x3f) << 16 | lo];
    return rd8_slow(a);
  }
  return g_rom[(bank & 0x3f) << 16 | lo];                // 40-7D, C0-FF
}

static inline __attribute__((always_inline)) void wr8(uint32_t a, uint8_t v) {
  uint32_t bank = a >> 16, lo = a & 0xffff;
  if ((bank & 0xfe) == 0x7e) g_wram[a & 0x1ffff] = v;
  else if (!(bank & 0x40) && lo < 0x2000) g_wram[lo] = v;
  else wr8_slow(a, v);
}

// Bank 0 (the direct page, the stack): WRAM below $2000, I/O above.
static inline __attribute__((always_inline)) uint8_t rd8_b0(uint16_t a) { return a < 0x2000 ? g_wram[a] : rd8_slow(a); }
static inline __attribute__((always_inline)) void wr8_b0(uint16_t a, uint8_t v) {
  if (a < 0x2000) g_wram[a] = v;
  else wr8_slow(a, v);
}

#endif
