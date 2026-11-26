// Draws the SNES's picture with the GBA's video hardware, from the hardware
// model's state (src/snes_hw.h) after each frame of the game's logic.
//
// The mapping, for now:
// - SNES Mode 1 (BG1 and BG2 4bpp, BG3 2bpp) -> GBA Mode 0, SNES BG1-3 on
//   GBA BG0-2 (the GBA's lower-numbered layer wins a tie, as BG1 should).
//   Other modes (Mode 7: the Rareware logo) aren't drawn yet.
// - Tiles: converted from SNES planar to GBA packed 4bpp on demand, into a
//   cache of GBA tile slots: 1024 for BG1/BG2 (charblocks 0-1), 575 for BG3,
//   widened to 4bpp (charblock 2 and part of 3). VRAM's write counters say
//   when a cached tile went stale.
// - Maps: only what the 240x160 window shows, a tile row at a time at that
//   row's scroll; each GBA cell is rewritten only when the SNES map entry it
//   should show changed (scrolling: the column or row coming into view, which
//   is off screen until the scroll changes at VBlank). Tile slots count the
//   cells showing them, and are reused only when none do. Scrolling that
//   changes by line (HDMA) is applied with HBlank DMA.
// - Sprites: the SNES's 512 sprite tiles are kept converted in GBA OBJ VRAM
//   in the GBA's 2D layout (a SNES tile row of 16 is a GBA row of 32, the
//   second 16 a copy of the first, for sprites that wrap around the row);
//   OAM is translated every frame.
// - Palettes: the same 15-bit colors; BG3's 2bpp palettes are spread into
//   GBA palettes 8-15. The screen's brightness is applied to the colors.
// - Color math: adding the fixed color (the sky's gradient in the levels):
//   a plane of it under the backgrounds it's added to, GBA BG3, blended
//   additively, its color set line by line by HBlank DMA.
// - The SNES's 256x224 screen is cropped to 240x160 at a fixed place.
// Not yet: other color math, windows, per-tile priority, Mode 7, a crop
// that follows the player.
#include "render.h"

#include <stdbool.h>
#include <string.h>

#include "gba_hw.h"
#include "snes_hw.h"

#define CROP_X 8
#define CROP_Y 32
#define LINES 160

#define BG_VRAM ((volatile uint8_t *)0x06000000)
#define OBJ_VRAM ((volatile uint32_t *)0x06010000)
#define BG_PALETTE ((volatile uint16_t *)0x05000000)
#define OBJ_PALETTE ((volatile uint16_t *)0x05000200)
#define OAM ((volatile uint16_t *)0x07000000)

#define POOL_A_SLOTS 1024             // charblocks 0-1
#define POOL_B_BASE 0x8000            // charblock 2
#define POOL_B_SLOTS 576              // to 0xC800; the last is the fixed color's tile
#define PLANE_SLOT (POOL_B_SLOTS - 1)
#define EMPTY_SLOT_B (POOL_B_SLOTS - 2)   // a transparent tile, for cells left out
#define PLANE_SCREENBLOCK 25          // 0xC800: the fixed color's map
#define SCREENBLOCK(n) ((volatile uint16_t *)(0x06000000 + (n) * 0x800))
#define FIRST_SCREENBLOCK 26          // 26-28: the three BGs' maps

// ---- what the VBlank interrupt puts on screen ----

typedef struct {
  uint16_t oam[128 * 4];
  uint16_t palette[512];
  uint16_t scroll[LINES + 2][8];   // per line: BG0-3 HOFS, VOFS; line 0 twice, a spare at the end
  uint16_t fixed[LINES + 2];       // per line: the SNES's fixed color, the same way
  uint16_t dispcnt, bgcnt[4], bldcnt, bldalpha;
  bool per_line_scroll, per_line_fixed;
  uint32_t palette_key;            // CGRAM's write count and the brightness it was made for
} Frame;

EWRAM_DATA static Frame g_frames[2];
static volatile int g_pending = -1;   // a finished frame not yet shown
static volatile uint32_t g_vblanks;
static int g_shown;                    // the frame on screen

// ---- tile cache ----
//
// A GBA tile slot holds a converted SNES tile while map cells use it (refs);
// with no refs it stays in the cache until another tile needs the slot.

#define IWRAM_CODE __attribute__((section(".iwram"), target("arm"), noinline))

static uint32_t g_expand[256];          // a bitplane byte -> a bit in each pixel's nibble

typedef struct {
  uint16_t *slot_of;   // per SNES tile: slot + 1, or 0
  uint16_t *owner;     // per slot: SNES tile + 1, or 0
  uint16_t *refs;      // per slot: map cells showing it
  uint16_t nslots, cursor;
  uint32_t base;       // byte offset in BG VRAM
  uint8_t shift;       // SNES tile index = word address >> shift
  uint32_t full;       // times no slot was free
} Pool;

EWRAM_DATA static uint16_t g_slot_a[2048], g_owner_a[POOL_A_SLOTS], g_refs_a[POOL_A_SLOTS];
EWRAM_DATA static uint16_t g_slot_b[4096], g_owner_b[POOL_B_SLOTS], g_refs_b[POOL_B_SLOTS];
static Pool g_pool_a = {g_slot_a, g_owner_a, g_refs_a, POOL_A_SLOTS, 0, 0, 4, 0};
static Pool g_pool_b = {g_slot_b, g_owner_b, g_refs_b, EMPTY_SLOT_B, 0, POOL_B_BASE, 3, 0};

IWRAM_CODE static void convert_tile(const Pool *p, uint32_t tile, uint32_t slot) {
  volatile uint32_t *dst = (volatile uint32_t *)(BG_VRAM + p->base + slot * 32);
  const uint16_t *src = &g_hw.vram[(tile << p->shift) & 0x7fff];
  const uint32_t *ex = g_expand;
  if (p->shift == 4) {
    for (int r = 0; r < 8; r++) {
      uint32_t a = src[r], b = src[(r + 8) & 0x7fff];
      dst[r] = ex[a & 0xff] | ex[a >> 8] << 1 | ex[b & 0xff] << 2 | ex[b >> 8] << 3;
    }
  } else {
    for (int r = 0; r < 8; r++) {
      uint32_t a = src[r];
      dst[r] = ex[a & 0xff] | ex[a >> 8] << 1;
    }
  }
}

// A slot showing SNES tile `tile`, with one more ref; converts the tile if
// it isn't cached (cached tiles are kept fresh by refresh_dirty).
static void evict_hidden(void);

IWRAM_CODE static uint32_t acquire(Pool *p, uint32_t tile) {
  uint32_t s = p->slot_of[tile];
  if (s) {
    s--;
    p->refs[s]++;
    return s;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    uint32_t n = p->nslots, c = p->cursor;
    for (uint32_t tries = n; tries; tries--) {
      if (!p->refs[c]) {
        p->cursor = c + 1 == n ? 0 : c + 1;
        if (p->owner[c]) p->slot_of[p->owner[c] - 1] = 0;
        p->owner[c] = (uint16_t)(tile + 1);
        p->slot_of[tile] = (uint16_t)(c + 1);
        convert_tile(p, tile, c);
        p->refs[c] = 1;
        return c;
      }
      c = c + 1 == n ? 0 : c + 1;
    }
    // Every slot is shown somewhere: free the cells off screen and retry.
    evict_hidden();
  }
  p->full++;
  return 0;
}

static inline void release(Pool *p, uint32_t slot) {
  if (p->refs[slot]) p->refs[slot]--;
}

// A tile whose VRAM changed: reconverted if cells show it, else dropped
// from the cache.
IWRAM_CODE static void tile_changed(Pool *p, uint32_t tile) {
  uint32_t s = p->slot_of[tile];
  if (!s) return;
  s--;
  if (p->refs[s]) {
    convert_tile(p, tile, s);
  } else {
    p->slot_of[tile] = 0;
    p->owner[s] = 0;
  }
}

// ---- backgrounds ----
//
// Each GBA map cell remembers which SNES map entry it shows (its address
// and value); a frame rewrites only the cells whose entry changed: while
// scrolling, the column or row coming into view.

#define NO_KEY 0xffffffffu
EWRAM_DATA static uint32_t g_cell_key[3][1024];
static uint32_t g_bg_setup[3];          // what the cells were drawn for: map, size, chr, bpp
static uint32_t g_visible_rows[3];       // GBA map rows shown this frame (bits)
static uint8_t g_row_col0[3][32];        // the first GBA column shown in each

static uint16_t g_hofs[3][224], g_vofs[3][224];
static uint16_t g_fixed_line[224];

IWRAM_CODE static void capture_line(int line) {
  for (int b = 0; b < 3; b++) {
    g_hofs[b][line] = g_hw.hofs[b];
    g_vofs[b][line] = g_hw.vofs[b];
  }
  g_fixed_line[line] = (uint16_t)(g_hw.fixed_r | g_hw.fixed_g << 5 | g_hw.fixed_b << 10);
}

static Pool *bg_pool(int b) { return (g_bg_setup[b] >> 31) ? &g_pool_b : &g_pool_a; }

static void clear_cells(int b) {
  volatile uint16_t *map = SCREENBLOCK(FIRST_SCREENBLOCK + b);
  Pool *pool = bg_pool(b);
  for (int i = 0; i < 1024; i++) {
    if (g_cell_key[b][i] != NO_KEY) release(pool, map[i] & 0x3ff);
    g_cell_key[b][i] = NO_KEY;
    map[i] = 0;
  }
}

// Frees the cells not shown this frame, for slots.
static void evict_hidden(void) {
  for (int b = 0; b < 3; b++) {
    volatile uint16_t *map = SCREENBLOCK(FIRST_SCREENBLOCK + b);
    Pool *pool = bg_pool(b);
    for (int row = 0; row < 32; row++) {
      bool shown = (g_visible_rows[b] >> row) & 1;
      for (int col = 0; col < 32; col++) {
        int i = row * 32 + col;
        if (g_cell_key[b][i] == NO_KEY) continue;
        if (shown && ((col - g_row_col0[b][row]) & 31) < 31) continue;
        release(pool, map[i] & 0x3ff);
        g_cell_key[b][i] = NO_KEY;
      }
    }
  }
}

// The screen's brightness, applied to the palette: c * b / 15 per channel.
static uint8_t g_scale[16][32];

static uint16_t scale_color(uint16_t c, int b) {
  const uint8_t *t = g_scale[b];
  return (uint16_t)(t[c & 31] | t[(c >> 5) & 31] << 5 | t[(c >> 10) & 31] << 10);
}

IWRAM_CODE static void draw_row(int b, uint32_t row_base, uint32_t col0, bool wide, uint32_t chr,
                                uint32_t words_per_tile, uint16_t pal_add, volatile uint16_t *dst_row,
                                uint32_t *keys, Pool *pool, uint32_t need_priority) {
  const uint16_t *vram = g_hw.vram;
  uint32_t col_mask = wide ? 63 : 31;
  for (int k = 0; k < 31; k++) {
    uint32_t col = (col0 + k) & col_mask;
    uint32_t addr = (row_base + (col & 31) + ((col & 32) << 5)) & 0x7fff;
    uint32_t e = vram[addr];
    uint32_t key = addr << 16 | e;
    uint32_t cell = col & 31;
    if (keys[cell] == key) continue;
    if (keys[cell] != NO_KEY) release(pool, dst_row[cell] & 0x3ff);
    keys[cell] = key;
    if ((e & need_priority) != need_priority) {
      // A tile this layer doesn't show (see draw_bg).
      dst_row[cell] = EMPTY_SLOT_B;
      continue;
    }
    uint32_t tile = ((chr + (e & 0x3ff) * words_per_tile) & 0x7fff) >> pool->shift;
    uint32_t slot = acquire(pool, tile);
    dst_row[cell] = (uint16_t)(slot | (e & 0xc000) >> 4 | ((e >> 10) & 7) << 12 | pal_add);
  }
}

__attribute__((noinline)) static void draw_bg(int b, int bpp, Frame *f) {
  uint8_t sc = g_hw.regs[0x07 + b];
  uint32_t map_base = (uint32_t)(sc & 0xfc) << 8;     // words
  bool wide = sc & 1, tall = sc & 2;
  uint8_t nba = b < 2 ? g_hw.regs[0x0b] : g_hw.regs[0x0c];
  uint32_t chr = (uint32_t)((b & 1) ? nba >> 4 : nba & 0xf) << 12;   // words
  // With BGMODE's bit 3 in Mode 1, BG3's priority tiles go in front of
  // everything and the rest behind everything (behind the opaque sky, in the
  // levels): BG3 is drawn in front, with its priority tiles only.
  uint32_t need_priority = (b == 2 && (g_hw.regs[0x05] & 0x0f) == 0x09) ? 0x2000 : 0;
  uint32_t setup = (uint32_t)(bpp == 2) << 31 | (need_priority != 0) << 30 | sc << 16 | chr;
  if (setup != g_bg_setup[b]) {
    clear_cells(b);
    g_bg_setup[b] = setup;
  }
  Pool *pool = bg_pool(b);
  volatile uint16_t *map = SCREENBLOCK(FIRST_SCREENBLOCK + b);
  uint32_t words_per_tile = bpp * 4;
  uint16_t pal_add = bpp == 2 ? 8 << 12 : 0;
  uint32_t row_mask = tall ? 63 : 31, col_mask = wide ? 63 : 31;
  // Which SNES row, and which window of its columns, each GBA map row shows
  // this frame. A tile row normally goes in GBA row (row & 31); if its lines
  // scroll differently (HDMA: the clouds' bands), a second window of columns
  // goes in a spare GBA row (32 rows, ~21 on screen), and those lines' VOFS
  // point at it.
  int16_t row_of[32];
  uint8_t window_of[32];
  uint32_t natural = 0;
  for (int i = 0; i < 32; i++) row_of[i] = -1;
  for (int gy = 0; gy < LINES; gy++) {
    int sy = gy + CROP_Y;
    natural |= 1u << (((sy + g_vofs[b][sy] + 1) >> 3) & 31);
  }
  g_visible_rows[b] = 0;
  for (int gy = 0; gy < LINES; gy++) {
    int sy = gy + CROP_Y;
    uint32_t hofs = g_hofs[b][sy], vofs = g_vofs[b][sy];
    uint32_t bgy = sy + vofs + 1;
    uint32_t row = (bgy >> 3) & row_mask;
    uint32_t col0 = (hofs + CROP_X) >> 3;
    uint8_t window = (uint8_t)(col0 & col_mask);
    int g = row & 31;
    if (row_of[g] >= 0 && (row_of[g] != (int)row || window_of[g] != window)) {
      // This GBA row shows something else: another row already drawn for
      // these lines' window, or a spare one.
      int found = -1;
      for (int i = 0; i < 32 && found < 0; i++)
        if (row_of[i] == (int)row && window_of[i] == window) found = i;
      for (int i = 0; i < 32 && found < 0; i++)
        if (row_of[i] < 0 && !((natural >> i) & 1)) found = i;
      if (found >= 0) g = found;
    }
    if (row_of[g] != (int)row || window_of[g] != window) {
      row_of[g] = (int16_t)row;
      window_of[g] = window;
      g_visible_rows[b] |= 1u << g;
      g_row_col0[b][g] = (uint8_t)(col0 & 31);
      uint32_t row_base = map_base + (row & 31) * 32 + ((row & 32) ? (wide ? 0x800 : 0x400) : 0);
      draw_row(b, row_base, col0, wide, chr, words_per_tile, pal_add, map + g * 32,
               g_cell_key[b] + g * 32, pool, need_priority);
    }
    f->scroll[gy + 1][b * 2] = (uint16_t)((hofs + CROP_X) & 0x1ff);
    f->scroll[gy + 1][b * 2 + 1] = (uint16_t)((g * 8 + (bgy & 7) - gy) & 0x1ff);
  }
}

// ---- sprites ----

static uint32_t g_obj_base_seen = 0xffffffff, g_obj_gap_seen;

IWRAM_CODE static void convert_obj_tile(uint32_t t, uint32_t base, uint32_t gap) {
  uint32_t w = ((t < 256 ? base : base + gap) + (t & 255) * 16) & 0x7fff;
  const uint16_t *src = &g_hw.vram[w];
  const uint32_t *ex = g_expand;
  uint32_t row = t >> 4, col = t & 15;
  volatile uint32_t *d1 = OBJ_VRAM + (row * 32 + col) * 8;
  volatile uint32_t *d2 = OBJ_VRAM + (row * 32 + col + 16) * 8;
  for (int r = 0; r < 8; r++) {
    uint32_t a = src[r], b = src[(r + 8) & 0x7fff];
    uint32_t px = ex[a & 0xff] | ex[a >> 8] << 1 | ex[b & 0xff] << 2 | ex[b >> 8] << 3;
    d1[r] = px;
    d2[r] = px;
  }
}

// What VRAM writes changed since the last frame (snes_hw's dirty bits, a
// bit per 16 bytes): the background tiles cached from there, the sprite
// tiles there.
IWRAM_CODE static void refresh_dirty(uint32_t obj_base, uint32_t obj_gap) {
  uint32_t *dirty = g_hw.vram_dirty;
  uint32_t last_a = 0xffffffff, last_obj = 0xffffffff;   // 4bpp tiles span two chunks
  for (uint32_t w = 0; w < 128; w++) {
    uint32_t bits = dirty[w];
    if (!bits) continue;
    dirty[w] = 0;
    for (uint32_t k = 0; bits; k++, bits >>= 1) {
      if (!(bits & 1)) continue;
      uint32_t chunk = w * 32 + k;                // 8 words
      tile_changed(&g_pool_b, chunk);              // a 2bpp tile
      if (chunk >> 1 != last_a) {
        last_a = chunk >> 1;
        tile_changed(&g_pool_a, chunk >> 1);       // a 4bpp tile
      }
      uint32_t word = chunk * 8;
      uint32_t t0 = (word - obj_base) & 0x7fff, t1 = (word - obj_base - obj_gap) & 0x7fff;
      uint32_t t = t0 < 0x1000 ? t0 >> 4 : t1 < 0x1000 ? 256 + (t1 >> 4) : 0xffffffff;
      if (t != 0xffffffff && t != last_obj) {
        last_obj = t;
        convert_obj_tile(t, obj_base, obj_gap);
      }
    }
  }
}

// Priorities, without the SNES's per-tile priority yet. Mode 1's order is,
// front to back: BG3's priority tiles (with BGMODE's bit 3), sprites of
// priority 3, BG1's and BG2's priority tiles, sprites 2, BG1 and BG2, sprites
// 1, BG3 without the bit, sprites 0. On the GBA a sprite is in front of a
// background of the same priority, and BG0 in front of BG1 of the same, so:
// BG3 on top 0 (else 3), BG1 1, BG2 2, the fixed color's plane 3; sprites 3
// and 2 at 1 (in front of BG1: most of its tiles are low priority), 1 at 2,
// 0 at 3.
static const uint8_t kObjPriority[4] = {3, 2, 1, 1};

static const uint8_t kObjSizes[8][2] = {
  {8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {16, 32}, {16, 32},
};

__attribute__((noinline)) static void draw_sprites(Frame *f) {
  uint8_t obsel = g_hw.regs[0x01];
  uint32_t base = (uint32_t)(obsel & 7) << 13;
  uint32_t gap = (uint32_t)(((obsel >> 3) & 3) + 1) << 12;
  if (base != g_obj_base_seen || gap != g_obj_gap_seen) {
    for (uint32_t t = 0; t < 512; t++) convert_obj_tile(t, base, gap);
    g_obj_base_seen = base;
    g_obj_gap_seen = gap;
  }
  for (int i = 0; i < 128; i++) {
    uint16_t w0 = g_hw.oam[i * 2], w1 = g_hw.oam[i * 2 + 1];
    uint8_t high = (g_hw.high_oam[i >> 2] >> ((i & 3) * 2)) & 3;
    int size = kObjSizes[obsel >> 5][(high >> 1) & 1];
    int x = (w0 & 0xff) | (high & 1) << 8;
    if (x >= 256) x -= 512;
    int y = w0 >> 8;
    int sy = y - CROP_Y;   // the SNES shows a sprite from line Y (backgrounds from VOFS + 1)
    int gx = x - CROP_X;
    uint16_t *o = &f->oam[i * 4];
    // Off the window (with SNES's wrap at the bottom of the 256 lines).
    if (sy >= LINES) sy -= 256;
    if (gx <= -size || gx >= 240 || sy <= -size || sy >= LINES) {
      o[0] = 0x0200;   // hidden
      o[1] = o[2] = 0;
      continue;
    }
    uint32_t t = (w1 & 0xff) | (w1 & 0x100);
    uint32_t gtile = (t >> 4) * 32 + (t & 15);
    int gsize = size == 8 ? 0 : size == 16 ? 1 : size == 32 ? 2 : 3;
    uint8_t attr = w1 >> 8;
    o[0] = (uint16_t)(sy & 0xff);
    o[1] = (uint16_t)((gx & 0x1ff) | (attr & 0x40) << 6 | (attr & 0x80) << 6 | gsize << 14);
    o[2] = (uint16_t)(gtile | kObjPriority[(attr >> 4) & 3] << 10 | ((attr >> 1) & 7) << 12);
  }
}

// ---- the frame ----

void render_init(void) {
  for (int b = 0; b < 256; b++) {
    uint32_t e = 0;
    for (int x = 0; x < 8; x++) e |= (uint32_t)((b >> (7 - x)) & 1) << (4 * x);
    g_expand[b] = e;
  }
  // c * b / 15, as c * b * 2185 >> 15 (exact for c < 32, b < 16).
  for (int b = 0; b < 16; b++)
    for (int c = 0; c < 32; c++) g_scale[b][c] = (uint8_t)((c * b * 2185) >> 15);
  memset(g_frames, 0, sizeof(g_frames));
  g_frames[0].palette_key = g_frames[1].palette_key = NO_KEY;
  for (int b = 0; b < 3; b++) {
    for (int i = 0; i < 1024; i++) g_cell_key[b][i] = NO_KEY;
    g_bg_setup[b] = NO_KEY;
  }
  // The fixed color's plane: a tile of color 15, everywhere, in palette 15
  // (BG3's 2bpp palettes use only colors 0-3 of 8-15).
  volatile uint32_t *tile = (volatile uint32_t *)(BG_VRAM + POOL_B_BASE + PLANE_SLOT * 32);
  for (int r = 0; r < 8; r++) tile[r] = 0xffffffff;
  volatile uint32_t *empty = (volatile uint32_t *)(BG_VRAM + POOL_B_BASE + EMPTY_SLOT_B * 32);
  for (int r = 0; r < 8; r++) empty[r] = 0;
  for (int i = 0; i < 1024; i++) SCREENBLOCK(PLANE_SCREENBLOCK)[i] = (uint16_t)(PLANE_SLOT | 15 << 12);
  REG_DISPCNT = 0x0080;   // blank until the first frame
}

uint32_t g_render_cycles, g_idle_cycles;

void render_frame(void) {
  // The game may finish frames faster than the GBA shows them: wait for the
  // last one to go on screen (this also paces the game to 60 frames).
  uint32_t t0 = cycles_now();
  while (g_pending >= 0) {}
  uint32_t t1 = cycles_now();
  g_idle_cycles += t1 - t0;
  int fi = g_shown ^ 1;
  Frame *f = &g_frames[fi];

  // VRAM's changes since the last frame, into the cached tiles.
  {
    uint8_t obsel = g_hw.regs[0x01];
    uint32_t base = (uint32_t)(obsel & 7) << 13;
    uint32_t gap = (uint32_t)(((obsel >> 3) & 3) + 1) << 12;
    refresh_dirty(base, gap);
  }

  // HDMA writes the registers line by line; note the scroll at each line.
  hw_hdma_frame(capture_line);

  uint8_t inidisp = g_hw.regs[0x00], mode = g_hw.regs[0x05] & 7, tm = g_hw.regs[0x2c];
  uint8_t ts = g_hw.regs[0x2d], cgadsub = g_hw.regs[0x31];
  int bright = inidisp & 15;
  uint16_t dispcnt = 0x1000;   // Mode 0, 2D sprites, OBJ on
  if (mode == 1 || mode == 0) {
    int bpp[3] = {4, 4, 2};
    if (mode == 0) bpp[0] = bpp[1] = 2;
    for (int b = 0; b < 3; b++) {
      int sb = FIRST_SCREENBLOCK + b;
      if (tm & (1 << b)) {
        draw_bg(b, bpp[b], f);
        dispcnt |= 0x100 << b;
      }
      uint32_t char_base = bpp[b] == 4 ? 0 : POOL_B_BASE / 0x4000;
      f->bgcnt[b] = (uint16_t)((b + 1) | char_base << 2 | sb << 8);
    }
    // Mode 1 with BG3's priority bit: BG3's priority tiles go in front of
    // everything. All of BG3 does, for now.
    if (mode == 1 && (g_hw.regs[0x05] & 8)) f->bgcnt[2] &= ~3;
  }

  // Color math, for now one case: adding the fixed color to some
  // backgrounds, with no background on the sub screen (where the sub
  // screen is empty, the SNES adds the fixed color). The GBA adds a layer
  // to the one under it: the fixed color is a plane under them (GBA BG3,
  // priority 3), whose color HBlank DMA sets line by line.
  f->bldcnt = 0;
  f->per_line_fixed = false;
  f->bgcnt[3] = (uint16_t)(3 | (POOL_B_BASE / 0x4000) << 2 | PLANE_SCREENBLOCK << 8);
  if ((mode == 1 || mode == 0) && (cgadsub & 0x07) && !(cgadsub & 0x80) && !(ts & 0x0f)) {
    f->bldcnt = (uint16_t)((cgadsub & 0x07) | 0x40 | 0x0800);
    f->bldalpha = (cgadsub & 0x40) ? 0x0808 : 0x1010;
    dispcnt |= 0x0800;
    for (int gy = 0; gy < LINES; gy++) f->fixed[gy + 1] = scale_color(g_fixed_line[gy + CROP_Y], bright);
    f->fixed[0] = f->fixed[1];
    for (int gy = 1; gy < LINES; gy++)
      if (f->fixed[gy + 1] != f->fixed[1]) { f->per_line_fixed = true; break; }
  }
  if (!(tm & 0x10)) dispcnt &= ~0x1000;
  draw_sprites(f);

  // Palettes: SNES BG colors 0-127, BG3's 2bpp palettes as 16-color ones,
  // and the sprite colors, with the screen's brightness applied. Rebuilt
  // when CGRAM or the brightness changed since this buffer's last frame.
  uint32_t palette_key = (uint32_t)g_hw.cgram_ver << 4 | bright;
  if (f->palette_key != palette_key) {
    f->palette_key = palette_key;
    for (int i = 0; i < 128; i++) f->palette[i] = scale_color(g_hw.cgram[i], bright);
    for (int p = 0; p < 8; p++)
      for (int c = 0; c < 4; c++) f->palette[(8 + p) * 16 + c] = scale_color(g_hw.cgram[p * 4 + c], bright);
    for (int i = 0; i < 128; i++) f->palette[256 + i] = scale_color(g_hw.cgram[128 + i], bright);
  }
  f->palette[255] = f->fixed[0];

  if (inidisp & 0x80) dispcnt |= 0x80;
  f->dispcnt = dispcnt;

  // Line 0's scroll is set at VBlank; HBlank DMA sets each next line's.
  for (int k = 0; k < 8; k++) f->scroll[0][k] = f->scroll[1][k];
  f->per_line_scroll = false;
  for (int gy = 1; gy < LINES && !f->per_line_scroll; gy++)
    for (int k = 0; k < 6; k++)
      if (f->scroll[gy + 1][k] != f->scroll[1][k]) { f->per_line_scroll = true; break; }

  g_pending = fi;
  g_render_cycles += cycles_now() - t1;
}

// ---- VBlank ----

#define REG_DMA1SAD (*(volatile uint32_t *)0x040000bc)
#define REG_DMA1DAD (*(volatile uint32_t *)0x040000c0)
#define REG_DMA1CNT (*(volatile uint32_t *)0x040000c4)
#define REG_DMA0SAD (*(volatile uint32_t *)0x040000b0)
#define REG_DMA0DAD (*(volatile uint32_t *)0x040000b4)
#define REG_DMA0CNT (*(volatile uint32_t *)0x040000b8)
#define REG_DMA3SAD (*(volatile uint32_t *)0x040000d4)
#define REG_DMA3DAD (*(volatile uint32_t *)0x040000d8)
#define REG_DMA3CNT (*(volatile uint32_t *)0x040000dc)

static void dma3_copy32(volatile void *dst, const void *src, uint32_t words) {
  REG_DMA3SAD = (uint32_t)src;
  REG_DMA3DAD = (uint32_t)dst;
  REG_DMA3CNT = words | 0x84000000u;   // enable, 32-bit
}

void render_hold(int frames) {
  while (g_pending >= 0) {}
  uint32_t v = g_vblanks;
  while (g_vblanks - v < (uint32_t)frames) {}
}

void render_vblank(void) {
  g_vblanks++;
  REG_DMA0CNT = 0;
  REG_DMA1CNT = 0;
  if (g_pending >= 0) {
    g_shown = g_pending;
    const Frame *f = &g_frames[g_shown];
    dma3_copy32(OAM, f->oam, sizeof(f->oam) / 4);
    dma3_copy32(BG_PALETTE, f->palette, sizeof(f->palette) / 4);
    for (int b = 0; b < 4; b++) REG16(0x04000008 + b * 2) = f->bgcnt[b];
    REG16(0x04000050) = f->bldcnt;
    REG16(0x04000052) = f->bldalpha;
    REG_DISPCNT = f->dispcnt;
    g_pending = -1;
  }
  const Frame *f = &g_frames[g_shown];
  for (int k = 0; k < 8; k++) REG16(0x04000010 + k * 2) = f->scroll[0][k];
  if (f->per_line_scroll) {
    // After each line, the next line's eight scroll registers.
    REG_DMA0SAD = (uint32_t)&f->scroll[2][0];
    REG_DMA0DAD = 0x04000010;
    REG_DMA0CNT = 8 | 0x80000000u | 0x20000000u | 0x02000000u | 0x00600000u;
  }
  BG_PALETTE[255] = f->fixed[0];
  if (f->per_line_fixed) {
    // After each line, the next line's fixed color, into palette entry 255.
    REG_DMA1SAD = (uint32_t)&f->fixed[2];
    REG_DMA1DAD = 0x050001fe;
    REG_DMA1CNT = 1 | 0x80000000u | 0x20000000u | 0x02000000u | 0x00400000u;
  }
}
