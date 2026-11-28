// The GBA build. For now it runs the game's logic on the hardware model and
// reports, through mGBA's debug output: the cycles each SNES frame takes,
// and (built with SYNC_LOG) a hash of WRAM per frame, to compare with the
// reference (tools/compare_sync.py). Nothing is drawn or played yet.
#include <stdbool.h>
#include <stdint.h>

#include "cpu65.h"
#include "game_loop.h"
#include "gba_hw.h"
#include "render.h"
#include "snes_hw.h"
#include "sync_hash.h"

extern const uint32_t g_snes_rom_offset;   // crt0.S, set by make_cartridge.py

static int g_frame;


static uint16_t read_joypad(void);

volatile uint8_t g_lag_event, g_frame_vblanks;

void irq_handler(void) {
  uint16_t flags = REG_IF;
  if (flags & 2) render_hblank();
  if (flags & 1) {
    render_vblank();
    if (++g_frame_vblanks > LAG_VBLANKS) g_lag_event = 1;
  }
  REG_IF = flags;
  REG_BIOS_IF |= flags;
}

#ifdef TEST_KEYS
#include "test_keys.h"   // generated from DKC2_TEST_KEYS: g_test_keys[][2]
#endif
static int g_waits;

// The view (render.h): the SNES's whole picture, squeezed, except in the
// levels, where the GBA shows LEVEL_LINES of its 224 lines: 184 squeezed
// into 160 are as wide for their height as a TV shows the SNES's (its
// pixels are 8:7). The window keeps the Kong in a band in its middle, so it
// stays put through ordinary jumps.
#ifndef LEVEL_LINES
#define LEVEL_LINES 184
#endif
#define WRAM16(a) (g_wram[a] | g_wram[(a) + 1] << 8)
#define GAMEMODE_LEVEL 0x8819     // gamemode_pointer ($24): the level's loop
#define ACTIVE_KONG 0x0593        // the active Kong's sprite (in WRAM)
#define CAMERA_Y 0x17c2
#define BAND_TOP (LEVEL_LINES * 7 / 20)      // the Kong's feet stay in this
#define BAND_BOTTOM (LEVEL_LINES * 4 / 5)    // part of the window (SNES lines)
#define CROP_STEP 4               // lines a frame the window moves at most

static void choose_view(void) {
  static int top = -1;
  uint16_t kong = WRAM16(ACTIVE_KONG);
  uint16_t sprite = kong >= 0x0de2 && kong < 0x16fe ? WRAM16(kong) : 0;
  if (WRAM16(0x24) != GAMEMODE_LEVEL || (sprite != 0x00e4 && sprite != 0x00e8)) {
    top = -1;
    render_set_view(0, 224);
    return;
  }
  // The sprite's Y is about where its feet are, in the level; less the
  // camera's, its line on the SNES's screen.
  int feet = (int16_t)(WRAM16(kong + 0x0a) - WRAM16(CAMERA_Y)) - 10;
  int want = top < 0 ? feet - (BAND_TOP + BAND_BOTTOM) / 2 : top;
  if (feet < want + BAND_TOP) want = feet - BAND_TOP;
  if (feet > want + BAND_BOTTOM) want = feet - BAND_BOTTOM;
  if (want < 0) want = 0;
  if (want > 224 - LEVEL_LINES) want = 224 - LEVEL_LINES;
  if (top >= 0) {
    if (want > top + CROP_STEP) want = top + CROP_STEP;
    if (want < top - CROP_STEP) want = top - CROP_STEP;
  }
  top = want;
  render_set_view(top, LEVEL_LINES);
}

// Pacing and frame skipping. The game's frame n is due at VBlank n, 60 a
// second: the game never runs faster than on the SNES, and when it's more
// than a frame late, its picture isn't drawn (the game runs on at its own
// speed, the picture at no less than half of it: never more than
// MAX_SKIPPED in a row). A frame far behind (a slow one, loading) starts the
// count again rather than racing to catch up.
#define MAX_SKIPPED 1
#define MAX_BEHIND 4
EWRAM_DATA static uint32_t g_due_base, g_due;
EWRAM_DATA static int g_skipped;

static bool frame_due_drawn(bool must_draw) {
  uint32_t now = render_vblank_count() - g_due_base, due = g_due++;
  if (now > due + MAX_BEHIND) {
    g_due_base = render_vblank_count();
    g_due = 1;
    now = due = 0;
  }
  if (!must_draw && now > due + 1 && g_skipped < MAX_SKIPPED) {
    g_skipped++;
    return false;
  }
  g_skipped = 0;
  return true;
}

uint16_t on_wait(uint32_t wai_pc) {
  choose_view();
  bool shot = false;
#ifdef SHOT_LOG
  static const int kShots[] = {SHOT_LOG};
  for (unsigned i = 0; i < sizeof(kShots) / sizeof(kShots[0]); i++) shot |= kShots[i] == g_waits;
#endif
  if (frame_due_drawn(shot)) render_frame();
#ifdef SHOT_LOG
  // gbarun -t takes the picture in the frame after this one: keep it there.
  if (shot) {
    debug_printf("shot %d", g_waits);
    render_hold(2);
  }
#endif
#ifdef SYNC_LOG
  debug_printf("sync %d %08x %06x", g_frame, sync_hash(g_wram), wai_pc);
#else
  (void)wai_pc;
#endif
#ifdef TEST_KEYS
  // The script's frames are the game's own, as in snesrun.
  uint32_t n = g_waits++, at = 0;
  for (uint32_t i = 0; i < sizeof(g_test_keys) / sizeof(g_test_keys[0]); i++) {
    at += g_test_keys[i][0];
    if (n < at) return g_test_keys[i][1];
  }
  return 0;
#else
  g_waits++;
  return read_joypad();
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
  render_init();
  REG_DISPSTAT = 1 << 3;   // VBlank interrupt
  REG_IE = 1;
  REG_IME = 1;
  game_reset();
  uint32_t total = 0, worst = 0, window_vblanks = render_vblank_count();
  int in_window = 0;
  for (g_frame = 0;; g_frame++) {
    uint32_t t0 = cycles_now();
    bool ok = game_frame();
    uint32_t t = cycles_now() - t0;
    total += t;
    if (t > worst) worst = t;
    if (!ok) {
      debug_printf("frame %d: the game stopped", g_frame);
      for (;;) {}
    }
    if (++in_window == 60) {
      // Means over 60 frames, total / 60 by shifts (1/64 + 1/1024 + 1/16384:
      // 0.02% low). The game's logic is the frame's time less drawing and
      // waiting for VBlank.
#define MEAN60(t) (((t) >> 6) + ((t) >> 10) + ((t) >> 14))
      uint32_t logic = total - g_render_cycles - g_idle_cycles;
      // The game's speed: its 60 frames against the VBlanks they took.
      uint32_t vb = render_vblank_count(), took = vb - window_vblanks;
      window_vblanks = vb;
      debug_printf("frames %d-%d: logic %u, drawing %u, idle %u cycles a frame; slowest frame %u "
                   "(a GBA frame is 280896); in %u VBlanks (60: full speed)", g_frame - 59, g_frame,
                   MEAN60(logic), MEAN60(g_render_cycles), MEAN60(g_idle_cycles), worst, took);
      total = worst = 0;
      g_render_cycles = g_idle_cycles = 0;
      in_window = 0;
    }
  }
}
