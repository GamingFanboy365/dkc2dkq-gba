// Draws the SNES's picture with the GBA's video hardware, from the hardware
// model's state (src/snes_hw.h) after each frame of the game's logic.
//
// The mapping, for now:
// - SNES Mode 1 (BG1 and BG2 4bpp, BG3 2bpp) -> GBA Mode 0, SNES BG1-3 on
//   GBA BG0-2 (the GBA's lower-numbered layer wins a tie, as BG1 should).
//   Other modes (Mode 7: the Rareware logo) aren't drawn yet.
// - Tiles: converted from SNES planar to GBA packed 4bpp on demand, into a
//   cache of GBA tile slots, a pool per SNES BG over 1790 slots (see
//   NUM_SLOTS), BG3's 2bpp tiles widened to 4bpp. VRAM's write counters say
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
// - The SNES's 256x224 screen on the 240x160 one (render_set_view): all
//   224 lines, a GBA line showing every 1.4th SNES line (its scroll set by
//   HBlank DMA, as for HDMA), the sprites shrunk to match as affine sprites;
//   or 160 of its lines at full size, from a line main.c picks (in the
//   levels, around the Kong). Columns 8-247 either way.
// Not yet: other color math, windows, per-tile priority, Mode 7.
#include "render.h"

#include <stdbool.h>
#include <string.h>

#include "gba_hw.h"
#include "snes_hw.h"

#define CROP_X 8
#define LINES 160

// The view: which SNES line each GBA line shows. SNES lines top to
// top + lines - 1 on the GBA's 160: GBA line y shows SNES line
// top + y * lines / 160 (with more than 160, some lines are left out), the
// sprites shrunk to match by affine sprites.
static int g_view_top = -1, g_view_lines;
static int32_t g_view_scale;   // 160 / lines, 16.16
static uint8_t g_src_line[LINES];

// a / b (there's no division in this build's C library); rarely called.
static uint32_t udiv(uint32_t a, uint32_t b) {
  uint32_t q = 0, r = 0;
  for (int i = 31; i >= 0; i--) {
    r = r << 1 | ((a >> i) & 1);
    if (r >= b) { r -= b; q |= 1u << i; }
  }
  return q;
}

void render_set_view(int top, int lines) {
  if (lines < LINES) lines = LINES;
  if (lines > 224) lines = 224;
  if (top < 0) top = 0;
  if (top > 224 - lines) top = 224 - lines;
  if (top == g_view_top && lines == g_view_lines) return;
  g_view_top = top;
  g_view_lines = lines;
  g_view_scale = (int32_t)udiv(LINES << 16, lines);
  for (int y = 0; y < LINES; y++) g_src_line[y] = (uint8_t)(top + udiv(y * lines, LINES));
}

#define BG_VRAM ((volatile uint8_t *)0x06000000)
#define OBJ_VRAM ((volatile uint32_t *)0x06010000)
#define BG_PALETTE ((volatile uint16_t *)0x05000000)
#define OBJ_PALETTE ((volatile uint16_t *)0x05000200)
#define OAM ((volatile uint16_t *)0x07000000)

// BG VRAM: 1792 tile slots of 32 bytes (0x0000-0xDFFF), then the maps.
// Each SNES BG has its own pool of slots and a GBA char base, so it can
// address 1024 of them: BG1 slots 0-1023 (charblock 0), BG2 512-1535
// (charblock 1), BG3 1024-1789 (charblock 2). The fixed color's plane
// uses charblock 3.
#define NUM_SLOTS 1792
#define EMPTY_SLOT 1790               // a transparent tile, for cells left out
#define PLANE_SLOT 1791               // color 15 everywhere: the fixed color's
#define PLANE_SCREENBLOCK 28          // 0xE000: the fixed color's map
#define SCREENBLOCK(n) ((volatile uint16_t *)(0x06000000 + (n) * 0x800))
#define FIRST_SCREENBLOCK 29          // 29-31: the three BGs' maps

// ---- what the VBlank interrupt puts on screen ----

#define MAX_LINE_COLORS 4             // CGRAM entries HDMA may rewrite, followed

typedef struct {
  uint16_t oam[128 * 4];
  uint16_t palette[512];
  uint16_t scroll[LINES + 2][8];   // per line: BG0-3 HOFS, VOFS; line 0 twice, a spare at the end
  uint16_t fixed[LINES + 2];       // per line: the SNES's fixed color, the same way
  uint16_t dispcnt, bgcnt[4], bldcnt, bldalpha;
  bool per_line_scroll, per_line_fixed, per_line_regs;
  // Per line, when the layers or color math change down the screen
  // (per_line_regs): DISPCNT, BLDCNT, BLDALPHA, the three BGs' priorities
  // (2 bits each, for BG0-2CNT), and the colors HDMA rewrites (each in GBA
  // palette entries pal_index[k][0] and, if not 0, [1]), set by the HBlank
  // interrupt.
  uint16_t line_regs[LINES][4];
  uint16_t bgcnt_base[3];
  uint8_t num_line_colors;
  uint16_t pal_index[MAX_LINE_COLORS][2];
  uint16_t line_pal[LINES][MAX_LINE_COLORS];
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

// The order a pool takes free slots in: segments of slots counting up
// (step 1) or down (-1) from `first`. Each fills from its own end toward
// the slots it shares with its neighbors, so they meet only on screens that
// need most of them. A slot is free when no map cell shows it (it may still
// cache a tile); a pool takes the first free one in its order.
typedef struct {
  int16_t first, count, step;
} Segment;

typedef struct {
  uint16_t *slot_of;   // per SNES tile: slot + 1, or 0
  Segment seg[2];
  uint16_t hint;       // the slots before this place in the order aren't free
  uint16_t index_base; // the slot a GBA map's tile 0 is (its char base)
  uint8_t id, shift;   // SNES tile index = word address >> shift
  uint32_t full;       // times no slot was free
} Pool;

// Per slot: the pool and SNES tile it holds ((id << 13 | tile) + 1, or 0),
// and the map cells showing it.
EWRAM_DATA static uint16_t g_owner[NUM_SLOTS], g_refs[NUM_SLOTS];
EWRAM_DATA static uint16_t g_slot_of[3][4096];
static Pool g_pools[3] = {
  {g_slot_of[0], {{0, 1024, 1}, {0, 0, 1}}, 0, 0, 0, 4, 0},
  {g_slot_of[1], {{1024, 512, 1}, {1023, 512, -1}}, 0, 512, 1, 4, 0},
  {g_slot_of[2], {{1536, 254, 1}, {1535, 512, -1}}, 0, 1024, 2, 3, 0},
};

static inline uint32_t order_len(const Pool *p) { return (uint32_t)(p->seg[0].count + p->seg[1].count); }

static inline uint32_t slot_at(const Pool *p, uint32_t pos) {
  const Segment *g = &p->seg[0];
  if (pos >= (uint32_t)g->count) { pos -= (uint32_t)g->count; g = &p->seg[1]; }
  return (uint32_t)(g->first + g->step * (int)pos);
}

// A slot's place in a pool's order, or -1.
static inline int place_of(const Pool *p, uint32_t slot) {
  int base = 0;
  for (int k = 0; k < 2; k++) {
    const Segment *g = &p->seg[k];
    int d = ((int)slot - g->first) * g->step;
    if (d >= 0 && d < g->count) return base + d;
    base += g->count;
  }
  return -1;
}

IWRAM_CODE static void convert_tile(const Pool *p, uint32_t tile, uint32_t slot) {
  volatile uint32_t *dst = (volatile uint32_t *)(BG_VRAM + slot * 32);
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
// it isn't cached (cached tiles are kept fresh by refresh_dirty). Returns
// the GBA map's tile index for it.
static void evict_hidden(void);

IWRAM_CODE static uint32_t acquire(Pool *p, uint32_t tile) {
  uint32_t s = p->slot_of[tile];
  if (s) {
    s--;
    g_refs[s]++;
    return s - p->index_base;
  }
  for (int attempt = 0; attempt < 2; attempt++) {
    for (uint32_t pos = p->hint, n = order_len(p); pos < n; pos++) {
      uint32_t slot = slot_at(p, pos);
      if (g_refs[slot]) continue;
      p->hint = (uint16_t)(pos + 1);
      uint32_t o = g_owner[slot];
      if (o) g_pools[(o - 1) >> 13].slot_of[(o - 1) & 0x1fff] = 0;
      g_owner[slot] = (uint16_t)((p->id << 13 | tile) + 1);
      p->slot_of[tile] = (uint16_t)(slot + 1);
      convert_tile(p, tile, slot);
      g_refs[slot] = 1;
      return slot - p->index_base;
    }
    // Every slot is shown somewhere: free the cells off screen and retry.
    evict_hidden();
  }
  p->full++;
  return EMPTY_SLOT - p->index_base < 1024 ? EMPTY_SLOT - p->index_base : 0;
}

// A map cell's tile index -> its slot, one ref less.
static inline void release(Pool *p, uint32_t index) {
  uint32_t slot = index + p->index_base;
  if (slot >= NUM_SLOTS || !g_refs[slot] || --g_refs[slot]) return;
  for (int i = 0; i < 3; i++) {
    int pos = place_of(&g_pools[i], slot);
    if (pos >= 0 && pos < g_pools[i].hint) g_pools[i].hint = (uint16_t)pos;
  }
}

// A tile whose VRAM changed: reconverted if cells show it, else dropped
// from the cache.
IWRAM_CODE static void tile_changed(Pool *p, uint32_t tile) {
  uint32_t s = p->slot_of[tile];
  if (!s) return;
  s--;
  if (g_refs[s]) {
    convert_tile(p, tile, s);
  } else {
    p->slot_of[tile] = 0;
    g_owner[s] = 0;
  }
}

// Empties a pool's cache (its tiles change size); its cells are released.
static void flush_pool(Pool *p) {
  for (uint32_t s = 0; s < NUM_SLOTS; s++)
    if (g_owner[s] && (uint32_t)((g_owner[s] - 1) >> 13) == p->id) g_owner[s] = 0;
  memset(p->slot_of, 0, sizeof(g_slot_of[0]));
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

// Per SNES line, what HDMA may change besides the scroll: the main and
// sub screens' layers, color math's settings, and up to MAX_LINE_COLORS
// CGRAM entries (for lines before an entry's first write, its value then:
// the hook sees a write before it's made).
static uint8_t g_line_tm[224], g_line_ts[224], g_line_cgwsel[224], g_line_cgadsub[224];
static uint8_t g_hdma_color[MAX_LINE_COLORS], g_num_hdma_colors;
static uint16_t g_line_color[224][MAX_LINE_COLORS];
static int g_hdma_line;

static void hdma_write_hook(uint8_t reg, uint8_t value) {
  (void)value;
  if (reg != 0x22 || !g_hw.cgram_second_write) return;
  uint8_t c = g_hw.cgram_pointer;
  for (int k = 0; k < g_num_hdma_colors; k++)
    if (g_hdma_color[k] == c) return;
  if (g_num_hdma_colors == MAX_LINE_COLORS) return;
  int k = g_num_hdma_colors++;
  g_hdma_color[k] = c;
  for (int line = 0; line < g_hdma_line; line++) g_line_color[line][k] = g_hw.cgram[c];
}

IWRAM_CODE static void capture_line(int line) {
  for (int b = 0; b < 3; b++) {
    g_hofs[b][line] = g_hw.hofs[b];
    g_vofs[b][line] = g_hw.vofs[b];
  }
  g_fixed_line[line] = (uint16_t)(g_hw.fixed_r | g_hw.fixed_g << 5 | g_hw.fixed_b << 10);
  g_line_tm[line] = g_hw.regs[0x2c];
  g_line_ts[line] = g_hw.regs[0x2d];
  g_line_cgwsel[line] = g_hw.regs[0x30];
  g_line_cgadsub[line] = g_hw.regs[0x31];
  for (int k = 0; k < g_num_hdma_colors; k++) g_line_color[line][k] = g_hw.cgram[g_hdma_color[k]];
  g_hdma_line = line + 1;
}

// The GBA palette entries showing SNES color c (render_frame's layout).
static int gba_entries(int c, uint16_t *out) {
  if (c >= 128) { out[0] = (uint16_t)(256 + c - 128); return 1; }
  out[0] = (uint16_t)c;
  if (c < 32) { out[1] = (uint16_t)((8 + c / 4) * 16 + c % 4); return 2; }
  return 1;
}

static Pool *bg_pool(int b) { return &g_pools[b]; }

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
      dst_row[cell] = (uint16_t)(EMPTY_SLOT - pool->index_base);
      continue;
    }
    uint32_t tile = ((chr + (e & 0x3ff) * words_per_tile) & 0x7fff) >> pool->shift;
    uint32_t slot = acquire(pool, tile);
    dst_row[cell] = (uint16_t)(slot | (e & 0xc000) >> 4 | ((e >> 10) & 7) << 12 | pal_add);
  }
}

__attribute__((noinline)) static void draw_bg(int b, int bpp, bool main_screen, Frame *f) {
  uint8_t sc = g_hw.regs[0x07 + b];
  uint32_t map_base = (uint32_t)(sc & 0xfc) << 8;     // words
  bool wide = sc & 1, tall = sc & 2;
  uint8_t nba = b < 2 ? g_hw.regs[0x0b] : g_hw.regs[0x0c];
  uint32_t chr = (uint32_t)((b & 1) ? nba >> 4 : nba & 0xf) << 12;   // words
  // With BGMODE's bit 3 in Mode 1, BG3's priority tiles go in front of
  // everything and the rest behind everything (behind the opaque sky, in the
  // levels): BG3 is drawn in front, with its priority tiles only.
  // (Only where BG3 is on the main screen: on the sub screen alone, it's
  // behind the main screen's layers anyway, and its other tiles are seen.)
  uint32_t need_priority = (b == 2 && (g_hw.regs[0x05] & 0x0f) == 0x09 && main_screen) ? 0x2000 : 0;
  uint32_t setup = (uint32_t)(bpp == 2) << 31 | (need_priority != 0) << 30 | sc << 16 | chr;
  Pool *pool = bg_pool(b);
  if (setup != g_bg_setup[b]) {
    clear_cells(b);
    g_bg_setup[b] = setup;
    uint8_t shift = bpp == 2 ? 3 : 4;
    if (pool->shift != shift) {
      flush_pool(pool);
      pool->shift = shift;
    }
  }
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
    int sy = g_src_line[gy];
    natural |= 1u << (((sy + g_vofs[b][sy] + 1) >> 3) & 31);
  }
  g_visible_rows[b] = 0;
  for (int gy = 0; gy < LINES; gy++) {
    int sy = g_src_line[gy];
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
  uint32_t last_4bpp = 0xffffffff, last_obj = 0xffffffff;   // 4bpp tiles span two chunks
  for (uint32_t w = 0; w < 128; w++) {
    uint32_t bits = dirty[w];
    if (!bits) continue;
    dirty[w] = 0;
    for (uint32_t k = 0; bits; k++, bits >>= 1) {
      if (!(bits & 1)) continue;
      uint32_t chunk = w * 32 + k;                // 8 words
      bool new_4bpp = chunk >> 1 != last_4bpp;
      last_4bpp = chunk >> 1;
      for (int i = 0; i < 3; i++) {
        Pool *p = &g_pools[i];
        if (p->shift == 3) tile_changed(p, chunk);                  // a 2bpp tile
        else if (new_4bpp) tile_changed(p, chunk >> 1);             // a 4bpp tile
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
  int top = g_view_top;
  bool fit = g_view_lines != LINES;
  for (int i = 0; i < 128; i++) {
    uint16_t w0 = g_hw.oam[i * 2], w1 = g_hw.oam[i * 2 + 1];
    uint8_t high = (g_hw.high_oam[i >> 2] >> ((i & 3) * 2)) & 3;
    int size = kObjSizes[obsel >> 5][(high >> 1) & 1];
    int x = (w0 & 0xff) | (high & 1) << 8;
    if (x >= 256) x -= 512;
    int y = w0 >> 8;
    int sy = y - top;   // the SNES shows a sprite from line Y (backgrounds from VOFS + 1)
    int gx = x - CROP_X;
    uint16_t *o = &f->oam[i * 4];
    // Off the window (with SNES's wrap at the bottom of the 256 lines).
    if (fit) {
      // An affine sprite draws its picture shrunk around the middle of its
      // box: the box goes where that puts SNES line Y at GBA line
      // (Y - top) * 160 / lines.
      if (y >= 224) y -= 256;
      sy = ((((2 * (y - top) + size) * g_view_scale) >> 16) - size) >> 1;
    }
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
    if (fit) {
      // Affine, with matrix 0-3 by the flips (see render_frame).
      o[0] = (uint16_t)((sy & 0xff) | 0x100);
      o[1] = (uint16_t)((gx & 0x1ff) | ((attr >> 6) & 3) << 9 | gsize << 14);
    } else {
      o[0] = (uint16_t)(sy & 0xff);
      o[1] = (uint16_t)((gx & 0x1ff) | (attr & 0x40) << 6 | (attr & 0x80) << 6 | gsize << 14);
    }
    o[2] = (uint16_t)(gtile | kObjPriority[(attr >> 4) & 3] << 10 | ((attr >> 1) & 7) << 12);
  }
}

// ---- the frame ----

void render_init(void) {
  render_set_view(0, 224);
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
  volatile uint32_t *tile = (volatile uint32_t *)(BG_VRAM + PLANE_SLOT * 32);
  for (int r = 0; r < 8; r++) tile[r] = 0xffffffff;
  volatile uint32_t *empty = (volatile uint32_t *)(BG_VRAM + EMPTY_SLOT * 32);
  for (int r = 0; r < 8; r++) empty[r] = 0;
  for (int i = 0; i < 1024; i++) SCREENBLOCK(PLANE_SCREENBLOCK)[i] = (uint16_t)((PLANE_SLOT - 1536) | 15 << 12);
  REG_DISPCNT = 0x0080;   // blank until the first frame
}

uint32_t g_render_cycles, g_idle_cycles;

IWRAM_CODE void render_frame(void) {
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

  // HDMA writes the registers line by line; note them at each line.
  g_num_hdma_colors = 0;
  g_hdma_line = 0;
  g_ppu_write_hook = hdma_write_hook;
  hw_hdma_frame(capture_line);
  g_ppu_write_hook = 0;

  uint8_t inidisp = g_hw.regs[0x00], mode = g_hw.regs[0x05] & 7;
  int bright = inidisp & 15;
  bool modes01 = mode == 1 || mode == 0;

  // The layers each shown line has on the main and sub screens, and its
  // color math. The SNES adds (or subtracts) the sub screen's pixel, or the
  // fixed color where the sub screen has none, to the main screen's, on the
  // layers CGADSUB picks. The GBA blends a pixel of its first targets with
  // the pixel under it, if that is a second target: the main screen's
  // layers with math are first targets; the sub screen's layers, put behind
  // the main screen's, are second targets, and so is a plane of the fixed
  // color under everything (GBA BG3, its color set line by line by HBlank
  // DMA), when the fixed color isn't black. Subtracting isn't done.
  uint8_t main_any = 0, sub_any = 0;
  bool plane = false;
  for (int gy = 0; gy < LINES; gy++) {
    int sy = g_src_line[gy];
    uint8_t cgadsub = g_line_cgadsub[sy], cgwsel = g_line_cgwsel[sy];
    bool math = modes01 && (cgadsub & 0x3f) && !(cgadsub & 0x80) && (cgwsel & 0x30) != 0x30;
    main_any |= g_line_tm[sy];
    if (math && (cgwsel & 2)) sub_any |= g_line_ts[sy] & ~g_line_tm[sy];
    if (math && g_fixed_line[sy]) plane = true;
  }
  main_any &= 0x17;
  sub_any &= 0x07;
  uint16_t dispcnt_base = 0;   // Mode 0, 2D sprites
  if (inidisp & 0x80) dispcnt_base |= 0x80;
  if (plane) dispcnt_base |= 0x0800;
  bool bg3_front = mode == 1 && (g_hw.regs[0x05] & 8);
  int bpp[3] = {4, 4, 2};
  if (mode == 0) bpp[0] = bpp[1] = 2;
  uint16_t bgcnt_base[3];
  for (int b = 0; b < 3; b++)
  {
    bgcnt_base[b] = (uint16_t)((uint32_t)b << 2 | (uint32_t)(FIRST_SCREENBLOCK + b) << 8);   // the pool's char base
    f->bgcnt_base[b] = bgcnt_base[b];
  }
  f->per_line_regs = false;
  uint32_t last_state = 0xffffffff;
  for (int gy = 0; gy < LINES; gy++) {
    int sy = g_src_line[gy];
    uint32_t state = (uint32_t)g_line_tm[sy] | g_line_ts[sy] << 8 | g_line_cgwsel[sy] << 16 |
                     (uint32_t)g_line_cgadsub[sy] << 24;
    if (state == last_state) {
      // As the line before: the same registers.
      uint16_t *r = f->line_regs[gy], *q = f->line_regs[gy - 1];
      r[0] = q[0]; r[1] = q[1]; r[2] = q[2]; r[3] = q[3];
      continue;
    }
    last_state = state;
    uint8_t tm = g_line_tm[sy] & 0x17, cgadsub = g_line_cgadsub[sy], cgwsel = g_line_cgwsel[sy];
    bool math = modes01 && (cgadsub & 0x3f) && !(cgadsub & 0x80) && (cgwsel & 0x30) != 0x30;
    // The sub screen's layers: those only there are shown behind the main
    // screen's; all are second targets (one on both is under the main
    // screen's layers in front of it, as on the sub screen).
    uint8_t sub_all = math && (cgwsel & 2) ? g_line_ts[sy] & 0x07 : 0;
    uint8_t sub = sub_all & ~tm;
    uint16_t layers = (uint16_t)((tm | sub) << 8);
    uint16_t bldcnt = 0, bldalpha = 0;
    if (math) {
      bldcnt = (uint16_t)((cgadsub & 0x07 & tm) | (cgadsub & 0x20) | 0x40 | sub_all << 8 | 0x2000);
      if (plane) bldcnt |= 0x0800;
      bldalpha = (cgadsub & 0x40) ? 0x0808 : 0x1010;
    }
    // The layers' GBA priorities: the main screen's as always (BG3 with
    // BGMODE's priority bit 0, else 3; BG1 1; BG2 2), the sub screen's
    // behind them in the SNES's order (BG3 with the bit in front), each
    // moved back where the GBA, which puts the lower-numbered BG in front
    // of one of the same priority, would put it in front of one it should
    // be behind.
    int prio[3];
    prio[0] = 1;
    prio[1] = 2;
    prio[2] = bg3_front ? 0 : 3;
    int deepest = 0;
    for (int b = 0; b < 3; b++)
      if ((tm & (1 << b)) && prio[b] > deepest) deepest = prio[b];
    static const uint8_t kOrder[2][3] = {{0, 1, 2}, {2, 0, 1}};   // front to back
    uint8_t placed = tm & 7;
    for (int i = 0; i < 3; i++) {
      int b = kOrder[bg3_front][i];
      if (!(sub & (1 << b))) continue;
      int pr = deepest;
      for (bool moved = true; moved && pr < 3;) {
        moved = false;
        for (int x = 0; x < 3; x++)
          if ((placed & (1 << x)) && prio[x] == pr && b < x) { pr++; moved = true; break; }
      }
      prio[b] = pr;
      deepest = pr;
      placed |= (uint8_t)(1 << b);
    }
    uint16_t *r = f->line_regs[gy];
    r[0] = dispcnt_base | layers;
    r[1] = bldcnt;
    r[2] = bldalpha;
    r[3] = (uint16_t)(prio[0] | prio[1] << 2 | prio[2] << 4);
    if (gy)
      for (int k = 0; k < 4; k++)
        if (r[k] != f->line_regs[0][k]) f->per_line_regs = true;
  }
  uint16_t dispcnt = f->line_regs[0][0];
  f->bldcnt = f->line_regs[0][1];
  f->bldalpha = f->line_regs[0][2];

  if (modes01) {
    for (int b = 0; b < 3; b++) {
      if ((main_any | sub_any) & (1 << b)) draw_bg(b, bpp[b], (main_any >> b) & 1, f);
      else g_visible_rows[b] = 0;   // its cells may go (evict_hidden)
      f->bgcnt[b] = (uint16_t)(bgcnt_base[b] | ((f->line_regs[0][3] >> (2 * b)) & 3));
    }
  } else {
    for (int b = 0; b < 3; b++) g_visible_rows[b] = 0;
    dispcnt &= 0x1080;   // sprites only (Mode 7 isn't drawn yet)
    f->per_line_regs = false;
  }

  f->per_line_fixed = false;
  f->bgcnt[3] = (uint16_t)(3 | 3 << 2 | PLANE_SCREENBLOCK << 8);
  if (plane) {
    for (int gy = 0; gy < LINES; gy++) f->fixed[gy + 1] = scale_color(g_fixed_line[g_src_line[gy]], bright);
    f->fixed[0] = f->fixed[1];
    for (int gy = 1; gy < LINES; gy++)
      if (f->fixed[gy + 1] != f->fixed[1]) { f->per_line_fixed = true; break; }
  }
  draw_sprites(f);
  // The sprite matrices of a view that leaves lines out, 0-3 for no flip, X,
  // Y, both: a step of lines / 160 texels a line, and -1 for a flip.
  int pd = (int)udiv(g_view_lines << 8, LINES);
  for (int m = 0; m < 4; m++) {
    f->oam[m * 16 + 3] = (uint16_t)((m & 1) ? -256 : 256);   // pa
    f->oam[m * 16 + 7] = 0;                                  // pb
    f->oam[m * 16 + 11] = 0;                                 // pc
    f->oam[m * 16 + 15] = (uint16_t)((m & 2) ? -pd : pd);    // pd
  }

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

  // The colors HDMA rewrites: line 0's in the palette, the rest by line.
  f->num_line_colors = (uint8_t)g_num_hdma_colors;
  for (int k = 0; k < g_num_hdma_colors; k++) {
    uint16_t idx[2] = {0, 0};
    int n = gba_entries(g_hdma_color[k], idx);
    f->pal_index[k][0] = idx[0];
    f->pal_index[k][1] = n > 1 ? idx[1] : 0;
    for (int gy = 0; gy < LINES; gy++) f->line_pal[gy][k] = scale_color(g_line_color[g_src_line[gy]][k], bright);
    for (int j = 0; j < n; j++) f->palette[idx[j]] = f->line_pal[0][k];
    if (!f->per_line_regs)
      for (int gy = 1; gy < LINES; gy++)
        if (f->line_pal[gy][k] != f->line_pal[0][k]) { f->per_line_regs = true; break; }
  }
  // The palette is remade when CGRAM changes; these entries were changed
  // in it for this frame only.
  if (f->num_line_colors) f->palette_key = NO_KEY;

  f->dispcnt = dispcnt;

  // Line 0's scroll is set at VBlank; HBlank DMA sets each next line's.
  for (int k = 0; k < 8; k++) f->scroll[0][k] = f->scroll[1][k];
  f->per_line_scroll = false;
  for (int gy = 1; gy < LINES && !f->per_line_scroll; gy++)
    for (int k = 0; k < 6; k++)
      if (f->scroll[gy + 1][k] != f->scroll[1][k]) { f->per_line_scroll = true; break; }

#ifdef RENDER_DEBUG
  static uint32_t full[3];
  if (g_pools[0].full != full[0] || g_pools[1].full != full[1] || g_pools[2].full != full[2]) {
    debug_printf("tile slots ran out: %u (BG1), %u (BG2), %u (BG3)", g_pools[0].full - full[0],
                 g_pools[1].full - full[1], g_pools[2].full - full[2]);
    for (int i = 0; i < 3; i++) full[i] = g_pools[i].full;
  }
#endif
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
  if (f->per_line_regs) {
    // Line 0's, which the last frame's HBlank interrupts changed.
    REG_DISPCNT = f->line_regs[0][0];
    REG16(0x04000050) = f->line_regs[0][1];
    REG16(0x04000052) = f->line_regs[0][2];
    for (int b = 0; b < 3; b++)
      REG16(0x04000008 + b * 2) = f->bgcnt_base[b] | ((f->line_regs[0][3] >> (2 * b)) & 3);
    for (uint32_t k = 0; k < f->num_line_colors; k++) {
      BG_PALETTE[f->pal_index[k][0]] = f->line_pal[0][k];
      if (f->pal_index[k][1]) BG_PALETTE[f->pal_index[k][1]] = f->line_pal[0][k];
    }
  }
  // The HBlank interrupt (render_hblank), for frames whose layers, color
  // math or colors change down the screen.
  if (f->per_line_regs) {
    REG_DISPSTAT |= 1 << 4;
    REG_IE |= 2;
  } else {
    REG_DISPSTAT &= ~(1 << 4);
    REG_IE &= ~2;
  }
  if (f->per_line_fixed) {
    // After each line, the next line's fixed color, into palette entry 255.
    REG_DMA1SAD = (uint32_t)&f->fixed[2];
    REG_DMA1DAD = 0x050001fe;
    REG_DMA1CNT = 1 | 0x80000000u | 0x20000000u | 0x02000000u | 0x00400000u;
  }
}

// In each line's HBlank, while the frame on screen needs it: the next
// line's layers, color math and rewritten colors.
IWRAM_CODE void render_hblank(void) {
  uint32_t v = REG_VCOUNT + 1;
  if (v >= LINES) return;
  const Frame *f = &g_frames[g_shown];
  if (!f->per_line_regs) return;
  const uint16_t *r = f->line_regs[v];
  REG_DISPCNT = r[0];
  REG16(0x04000050) = r[1];
  REG16(0x04000052) = r[2];
  uint32_t pr = r[3];
  REG16(0x04000008) = f->bgcnt_base[0] | (pr & 3);
  REG16(0x0400000a) = f->bgcnt_base[1] | ((pr >> 2) & 3);
  REG16(0x0400000c) = f->bgcnt_base[2] | ((pr >> 4) & 3);
  volatile uint16_t *pal = BG_PALETTE;
  for (uint32_t k = 0; k < f->num_line_colors; k++) {
    uint16_t c = f->line_pal[v][k];
    pal[f->pal_index[k][0]] = c;
    if (f->pal_index[k][1]) pal[f->pal_index[k][1]] = c;
  }
}
