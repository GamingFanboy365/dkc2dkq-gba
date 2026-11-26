// The GBA build. For now it runs the game's logic on the hardware model and
// reports, through mGBA's debug output: the cycles each SNES frame takes,
// and (built with SYNC_LOG) a hash of WRAM per frame, to compare with the
// reference (tools/compare_sync.py). Nothing is drawn or played yet.
#include <stdbool.h>
#include <stdint.h>

#include "cpu65.h"
#include "game_loop.h"
#include "gba_hw.h"
#include "snes_hw.h"

extern const uint32_t g_snes_rom_offset;   // crt0.S, set by make_cartridge.py

static int g_frame;

#ifdef SYNC_LOG
static uint32_t hash_ram(const uint8_t *p, uint32_t n) {
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < n; i++) h = (h ^ (i == 0x2c || i == 0x2d ? 0 : p[i])) * 16777619u;
  return h;
}
#endif

void on_wait(uint32_t wai_pc) {
#ifdef SYNC_LOG
  debug_printf("sync %d %08x %06x", g_frame, hash_ram(g_wram, sizeof(g_wram)), wai_pc);
#else
  (void)wai_pc;
#endif
}

void unknown_entry(uint32_t pc) {
  debug_printf("frame %d: no recompiled code to enter at %06x", g_frame, pc);
}

// The GBA's buttons as the SNES's (docs/controls.md): A jumps (SNES B),
// B runs and attacks (SNES Y), R teams up (SNES A), L or Select swaps (SNES
// Select).
static uint16_t read_joypad(void) {
  uint16_t k = ~REG_KEYINPUT & 0x3ff;
  uint16_t j = 0;
  if (k & KEY_A) j |= 0x8000;        // B
  if (k & KEY_B) j |= 0x4000;        // Y
  if (k & (KEY_SELECT | KEY_L)) j |= 0x2000;
  if (k & KEY_START) j |= 0x1000;
  if (k & KEY_UP) j |= 0x0800;
  if (k & KEY_DOWN) j |= 0x0400;
  if (k & KEY_LEFT) j |= 0x0200;
  if (k & KEY_RIGHT) j |= 0x0100;
  if (k & KEY_R) j |= 0x0080;        // A
  return j;
}

int main(void) {
  // ROM: 3 wait states first access, 1 sequential; prefetch on.
  REG_WAITCNT = 0x4317;
  debug_init();
  if (!g_snes_rom_offset) {
    debug_print("no SNES ROM in this cartridge: build it with tools/make_cartridge.py");
    for (;;) {}
  }
  g_rom = (const uint8_t *)(0x08000000 + g_snes_rom_offset);
  g_rom_mask = 0x3fffff;
  debug_printf("SNES ROM at %08x", (uint32_t)g_rom);
  cycles_start();
  game_reset();
  uint32_t total = 0, worst = 0;
  int in_window = 0;
  for (g_frame = 0;; g_frame++) {
    uint32_t t0 = cycles_now();
    bool ok = game_frame(read_joypad());
    uint32_t t = cycles_now() - t0;
    total += t;
    if (t > worst) worst = t;
    if (!ok) {
      debug_printf("frame %d: the game stopped", g_frame);
      for (;;) {}
    }
    if (++in_window == 60) {
      // The mean, total / 60, by shifts (1/64 + 1/1024 + 1/16384: 0.02% low).
      uint32_t mean = (total >> 6) + (total >> 10) + (total >> 14);
      debug_printf("frames %d-%d: %u cycles a frame on average, %u at most (a GBA frame is 280896)",
                   g_frame - 59, g_frame, mean, worst);
      total = worst = 0;
      in_window = 0;
    }
  }
}
