// Native versions of the game's hottest routines, in place of their
// recompiled code (tools/recomp/recomp.py's NATIVE emits those as
// R_xxxxxx_recomp, and these take their names). Each does what the 65816
// code does to memory, byte for byte, scratch variables and stack included
// (tools/compare_sync.py compares all of WRAM with the reference every
// frame), and leaves the CPU as the callers need it. Anything it doesn't
// handle (another entry point, another direct page or register width) goes
// to the recompiled code.
#include "cpu65.h"
#include "regions.h"

uint32_t R_b59f40_recomp(uint32_t entry);
uint32_t R_80e580_recomp(uint32_t entry);
uint32_t R_b5a8da_recomp(uint32_t entry);
uint32_t R_b5f5e1_recomp(uint32_t entry);
uint32_t R_bb8000_recomp(uint32_t entry);

// The direct page (D = 0, bank 0), absolute with DB = $B5, long.
static inline uint8_t d8(uint16_t a) { return rd8_b0(a); }
static inline uint16_t d16(uint16_t a) { return rd16_b0(a); }
static inline void s8(uint16_t a, uint8_t v) { wr8_b0(a, v); }
static inline void s16(uint16_t a, uint16_t v) { wr16_b0(a, v); }
static inline uint16_t b16(uint32_t a) { return rd16(0xb50000 + a); }
static inline void bs16(uint32_t a, uint16_t v) { wr16(0xb50000 + a, v); }

// [dp],y
static inline uint32_t ptr24(uint16_t dp) { return d16(dp) | (uint32_t)d8((uint16_t)(dp + 2)) << 16; }
static inline uint8_t ind8(uint32_t p, uint16_t y) { return rd8((p + y) & 0xffffff); }
static inline uint16_t ind16(uint32_t p, uint16_t y) { return ind8(p, y) | ind8(p, (uint16_t)(y + 1)) << 8; }

// ($56): DB = $B5.
static inline void ora_56(uint8_t m) {
  uint32_t a = 0xb50000 + d16(0x56);
  wr8(a, rd8(a) | m);
}

// ---- CODE_B59F40: the objects' sprites into the OAM buffer ----
//
// For each of the 25 objects in sprite_render_table: its animation frame's
// pieces (a header in bank BC: 16x16 pieces, then two groups of 8x8 ones,
// with tile numbers) at its place on screen, into OAM from $70 on, the
// high table's size and X bits through ($56); objects with frames of their
// own graphics (frame >= $2C4) also get a DMA request in $1730's list.
// The 8 writers (CODE_B5A214 ... B5A733): flipped in X, Y, both or not,
// and with or without the header's fourth word.

// 0: done (RTS); 1: OAM is full (the writer drops its return address and
// leaves the whole routine). *y_out: Y at the end (the header's length).
static int write_pieces(bool xflip, bool yflip, bool large, uint16_t *y_out) {
  uint32_t p40 = ptr24(0x40), p44 = ptr24(0x44);
  s16(0x36, ind16(p40, 0));
  s16(0x38, ind16(p40, 2));
  s16(0x3a, ind16(p40, 4));
  uint16_t y = 5;
  if (large) {
    s16(0x3c, ind16(p40, 6));
    y = 8;
  }
  // LDA $70; LSR; LSR; SEP; TAX; ADC $36: room for the 16x16 pieces?
  uint16_t a = d16(0x70);
  uint8_t c = (a >> 1) & 1;
  a >>= 2;
  uint16_t x = a;
  if (!((uint8_t)((a & 0xff) + d8(0x36) + c) & 0x80)) return 1;
  // TXA; LSR; LSR; AND #$1F; STA $56; TXA; AND #$03; TAX; LDA DATA_B5A190,x
  s8(0x56, ((x & 0xff) >> 2) & 0x1f);
  uint16_t xi = (uint16_t)((a & 0xff00) | (x & 3));
  s8(0x3e, rd8(0xb5a190 + xi));
  x = d16(0x70);
  c = 0;
  // The 16x16 pieces: two high-table bits each, from $3E's mask.
  for (;;) {
    uint8_t n = (uint8_t)(d8(0x36) - 1);
    s8(0x36, n);
    if (n & 0x80) break;
    uint8_t yb = ind8(p44, y);
    if (yflip) yb ^= 0xff;
    uint16_t v = (uint16_t)(yb + d16(0x74) + c);
    if (v < 0xf0) {
      s16((uint16_t)(x + 1), (uint16_t)(v - 0x10));
      uint8_t xb = ind8(p40, y);
      if (xflip) xb ^= 0xff;
      uint16_t xv = (uint16_t)(xb + d16(0x72));
      s8(x, (uint8_t)xv);
      uint8_t m = d8(0x3e);
      if (xv < 0x100) m &= 0xaa;
      ora_56(m);
      m = d8(0x3e);
      if (!(m & 0x80)) {
        m = (uint8_t)(m << 2);   // ASL; ASL (the carry they leave is cleared before it is read)
      } else {
        s8(0x56, (uint8_t)(d8(0x56) + 1));
        m = 3;
      }
      s8(0x3e, m);
      s16((uint16_t)(x + 2), d16(0x34));
      x += 4;
    }
    // CLC; LDA $34; INC; INC; BIT #$10; BEQ; ADC #$10: the next 16x16 tile
    uint32_t t = (uint16_t)(d16(0x34) + 2);
    c = 0;
    if (t & 0x10) {
      t += 0x10;
      c = (t >> 16) & 1;
    }
    s16(0x34, (uint16_t)t);
    y += 2;
  }
  // The two groups of 8x8 pieces: a size bit when X is past 255.
  for (int group = 0; group < 2; group++) {
    uint16_t count = group ? 0x39 : 0x37;
    // REP; TXA; LSR; LSR; SEP; ADC count: room for them?
    c = (x >> 1) & 1;
    if (!((uint8_t)(((x >> 2) & 0xff) + d8(count) + c) & 0x80)) {
      s16(0x70, x);
      return 1;
    }
    s8(0x34, (uint8_t)(d8(group ? 0x3a : 0x38) + d8(0x32)));
    if (!group) {
      if (xflip) s16(0x72, (uint16_t)(d16(0x72) + 8));
      if (yflip) s16(0x74, (uint16_t)(d16(0x74) + 8));
    }
    for (;;) {
      uint8_t n = (uint8_t)(d8(count) - 1);
      s8(count, n);
      if (n & 0x80) break;
      uint8_t yb = ind8(p44, y);
      if (yflip) yb ^= 0xff;
      uint16_t v = (uint16_t)(yb + d16(0x74));
      if (v < 0xf0) {
        s16((uint16_t)(x + 1), (uint16_t)(v - 0x10));
        uint8_t xb = ind8(p40, y);
        if (xflip) xb ^= 0xff;
        uint16_t xv = (uint16_t)(xb + d16(0x72));
        s8(x, (uint8_t)xv);
        if (xv & 0x100) {
          s16(0x54, x);
          uint16_t xi2 = (x & 0x1fc) >> 2;
          s8(0x56, (uint8_t)(xi2 >> 2));
          ora_56(rd8(0xb5a194 + xi2));
          x = d16(0x54);
        }
        s16((uint16_t)(x + 2), d16(0x34));
        x += 4;
      }
      s16(0x34, (uint16_t)(d16(0x34) + 1));
      y += 2;
    }
  }
  s16(0x70, x);
  *y_out = y;
  return 0;
}

// The return addresses the routine's JSRs push (they're overwritten by the
// PEA at its end, like the PHKs' bytes, so only that one is written).
static void b59f40_body(void) {
  uint8_t db = cpu.db;
  s8(0x56, (d8(0x70) >> 4) & 0x1f);
  s16(0x62, 0);
  wr16((uint32_t)db << 16 | 0x0638, 0);
  for (;;) {
    // CODE_B59F53
    if (d16(0x70) == 0x400) {
      wr8((uint32_t)db << 16 | 0x2100, 0x07);   // OAM full: screen on, full brightness
      return;
    }
    db = 0xb5;   // PHK; PLB
    uint16_t y = b16(0x16fe + d16(0x62));
    if (b16(y) == 0) goto next;
    uint16_t x = d16((uint16_t)(0x1a + y));
    if (b16(0x1730) >= d16(0x78) && x >= 0x2c5) x = d16((uint16_t)(0x16 + y));
    bs16(0x18 + y, x);
    if (!x) goto next;
    uint16_t v = rd16(0xbc8000 + x);
    s16(0x40, v);
    s16(0x44, (uint16_t)(v + 1));
    v = rd16(0xbc8002 + x);
    s16(0x42, v);
    s16(0x46, v);
    bs16(0x14 + y, (uint16_t)((v >> 8) + b16(0x02 + y)));
    uint16_t a = b16(0x1c + y);
    if (!(a & 0x8000)) {
      // On screen, from its position less the camera's?
      uint16_t t = (uint16_t)(b16(0x0a + y) - 0x80);
      uint16_t cam_y = b16(0x17c0);
      uint8_t c = t >= cam_y;
      t = (uint16_t)(t - cam_y);
      s16(0x74, t);
      if ((uint16_t)(t + 0x90 + c) >= 0x130) goto next;
      t = (uint16_t)(b16(0x06 + y) - b16(0x17ba) - 1);
      s16(0x72, t);
      if ((uint16_t)(t + 0x30) >= 0x160) goto next;
    } else if (a & 0x4000) {
      // CODE_B5A068: not drawn; noted in $063A's list if bit 13
      if (a & 0x2000) {
        uint16_t k = b16(0x0638);
        if (k < 0x10) {
          bs16(0x063a + k, y);
          bs16(0x0638, (uint16_t)(k + 2));
        }
      }
      bs16(0x18 + y, b16(0x16 + y));
      goto next;
    } else if (a & 0x000f) {
      // CODE_B59FDD: with an offset (signed bytes $51 and $50)
      uint16_t o = b16(0x50 + y);
      o = (o & 0x8000) ? (uint16_t)(0xff00 | o >> 8) : (uint16_t)(o >> 8);
      uint16_t t = (uint16_t)(o + b16(0x0a + y) - 0x80);
      uint16_t cam_y = b16(0x17c0);
      uint8_t c = t >= cam_y;
      t = (uint16_t)(t - cam_y);
      s16(0x74, t);
      if ((uint16_t)(t + 0x90 + c) >= 0x130) goto next;
      o = b16(0x4f + y);
      o = (o & 0x8000) ? (uint16_t)(0xff00 | o >> 8) : (uint16_t)(o >> 8);
      uint32_t r = (uint32_t)o + b16(0x06 + y);
      c = (r >> 16) & 1;
      t = (uint16_t)((uint16_t)r - b16(0x17ba) - (1 - c));
      s16(0x72, t);
      if ((uint16_t)(t + 0x30) >= 0x160) goto next;
    } else {
      // On the screen's coordinates already
      s16(0x72, b16(0x06 + y));
      s16(0x74, (uint16_t)(b16(0x0a + y) - 0x80));
    }
    // CODE_B5A03C
    a = b16(0x12 + y);
    s16(0x32, a);
    s16(0x34, a);
    bool yflip = a & 0x8000, xflip = a & 0x4000, large = x >= 0x2c4;
    s16(0x72, (uint16_t)(d16(0x72) - (xflip ? 0x8f : 0x80)));
    s16(0x74, (uint16_t)(d16(0x74) + (yflip ? 1 : 0x10)));
    uint16_t yh;
    if (write_pieces(xflip, yflip, large, &yh)) return;
    if (large) {
      // CODE_B5A0E4: its graphics, into the DMA list, if they changed
      if (b16(0x1730) >= d16(0x78)) goto next;
      uint16_t o = b16(0x16fe + d16(0x62));
      uint16_t f = d16((uint16_t)(0x1a + o));
      if (f == d16((uint16_t)(0x16 + o))) goto next;
      s16((uint16_t)(0x16 + o), f);
      uint16_t k = b16(0x1730);
      uint16_t src = (uint16_t)(yh + d16(0x40));
      bs16(0x1736 + k, src);
      uint16_t len = (uint16_t)(d8(0x3b) << 5);
      bs16(0x1732 + k, len);
      s16(0x54, len);
      src = (uint16_t)(src + len);
      bs16(0x1734 + k, (uint16_t)((d16(0x32) & 0x1ff) << 4));
      bs16(0x1738 + k, d16(0x42) | 0xff00);
      k += 8;
      bs16(0x1738 + k, 0);
      uint16_t len2 = d16(0x3d) & 0x000f;
      if (len2) {
        bs16(0x1732 + k, (uint16_t)(len2 << 5));
        s16(0x54, (uint16_t)(d8(0x3c) << 4));
        bs16(0x1734 + k, (uint16_t)(((d16(0x32) & 0x1ff) << 4) + d16(0x54)));
        bs16(0x1736 + k, src);
        bs16(0x1738 + k, d16(0x42) | 0xff00);
        k += 8;
      }
      bs16(0x1730, k);
      bs16(0x1738 + k, 0);
    }
  next:
    s16(0x62, (uint16_t)(d16(0x62) + 2));
    if (d16(0x62) == 0x32) return;
  }
}

uint32_t R_b59f40(uint32_t entry) {
  if (entry != 0xb59f40 || cpu.d != 0 || cpu.e || cpu.mf || cpu.xf) return R_b59f40_recomp(entry);
  b59f40_body();
  // PEA $8080; PLB; PLB; RTL
  wr8_b0(cpu.s, 0x80);
  wr8_b0((uint16_t)(cpu.s - 1), 0x80);
  cpu.db = 0x80;
  cpu.n = 1;
  cpu.z = 0;
  cpu.mf = 0;
  uint16_t r = pull16();
  uint32_t b = pull8();
  return b << 16 | (uint16_t)(r + 1);
}

// ---- CODE_80E580: the levels' parallax tables ----
//
// HDMA's tables for the far background's bands, from the frame counter and
// the camera: three bytes of speeds, 17 runs of 8 scroll words stepping by
// a slope, then counters in the table bumped by the frame.

static inline uint16_t w16(uint32_t a) { return rd16(0x7e0000 + a); }
static inline void ws16(uint32_t a, uint16_t v) { wr16(0x7e0000 + a, v); }

uint32_t R_80e580(uint32_t entry) {
  if (entry != 0x80e580 || cpu.d != 0 || cpu.e || cpu.mf || cpu.xf) return R_80e580_recomp(entry);
  uint16_t fc = d16(0x2a);
  uint16_t a = fc >> 1;
  ws16(0x8013, a);
  a >>= 1;
  ws16(0x8016, a);
  a >>= 2;
  ws16(0x8019, a);
  a = (uint16_t)(rd16((uint32_t)cpu.db << 16 | 0x17ba) + fc);
  s16(0x36, a);
  uint16_t half = a >> 1;
  s16(0x32, half);
  a = (uint16_t)((half >> 1) + half + d16(0x36));
  uint8_t c = a >= 0x2000;   // SEC; SBC #$2000
  a = (uint16_t)((uint16_t)(a - 0x2000) ^ 0xffff);
  s16(0x32, a);
  uint16_t slope = a;
  a = d16(0x35) & 0xff00;
  uint16_t y = 0;
  for (int x = 0x100; x >= 0; x -= 0x10) {
    for (int k = 0; k < 8; k++) {
      ws16(0x802d - 2 * k + x, a);
      uint32_t r = (uint32_t)a + slope + c;
      a = (uint16_t)r;
      c = (r >> 16) & 1;
    }
    y = a;
    c = 1;   // CPX #$0000
  }
  // PEA $807E; PLB: DB = $7E
  wr8_b0(cpu.s, 0x80);
  wr8_b0((uint16_t)(cpu.s - 1), 0x7e);
  a = (uint16_t)(((rd16(0x00002a) >> 1) & 0x000e) + 0x90);
  int16_t x;
  for (;;) {
    x = (int16_t)a;
    static const uint8_t kIncs[] = {0x7e, 0x80, 0x82, 0x82, 0x84, 0x84, 0x86, 0x88};
    for (int k = 0; k < 8; k++) ws16(0x8000 + kIncs[k] + (uint16_t)x, (uint16_t)(w16(0x8000 + kIncs[k] + (uint16_t)x) + 1));
    a = (uint16_t)(x - 0x10);
    if (a & 0x8000) break;
  }
  a = (uint16_t)(x + 0x50);
  for (;;) {
    x = (int16_t)a;
    for (int k = 0; k < 4; k++) {
      uint32_t ad = 0x801e + 2 * k + (uint16_t)x;
      ws16(ad, (uint16_t)(w16(ad) + 1));
    }
    a = (uint16_t)(x - 0x10);
    if (a & 0x8000) break;
  }
  // PLB: DB = $80 (the PEA's other byte); RTS
  cpu.db = 0x80;
  cpu.a = a;
  cpu.x = (uint16_t)x;
  cpu.y = y;
  cpu.c = 0;
  cpu.v = 0;
  cpu.n = 1;
  cpu.z = 0;
  uint16_t r = pull16();
  return (uint32_t)cpu.pb << 16 | (uint16_t)(r + 1);
}

// ---- CODE_B5A8DA: sprite_render_table sorted by the objects' $14 ----
//
// One pass of a cocktail sort, forward then back, comparing each entry's
// key with the last one read ($54).

uint32_t R_b5a8da(uint32_t entry) {
  if (entry != 0xb5a8da || cpu.d != 0 || cpu.e || cpu.mf || cpu.xf) return R_b5a8da_recomp(entry);
  uint32_t db = (uint32_t)cpu.db << 16;
  uint16_t y = rd16(db | 0x16fe), a = rd16(db + 0x14 + y);
  s16(0x54, a);
  uint16_t x = 0x16fc;
  do {
    y = d16((uint16_t)(0x04 + x));
    a = rd16(db + 0x14 + y);
    uint16_t last = d16(0x54);
    s16(0x54, a);
    if (a > last) {
      a = d16((uint16_t)(0x02 + x));
      s16((uint16_t)(0x04 + x), a);
      s16((uint16_t)(0x02 + x), y);
    }
    x += 2;
  } while (x != 0x172c);
  do {
    y = d16(x);
    a = rd16(db + 0x14 + y);
    uint16_t last = d16(0x54);
    s16(0x54, a);
    if (a < last) {
      a = d16((uint16_t)(0x02 + x));
      s16(x, a);
      s16((uint16_t)(0x02 + x), y);
    }
    x -= 2;
  } while (x != 0x16fc);
  // CPX #$16FC: equal; RTL
  cpu.a = a;
  cpu.x = x;
  cpu.y = y;
  cpu.n = 0;
  cpu.z = 1;
  cpu.c = 1;
  uint16_t r = pull16();
  uint32_t b = pull8();
  return b << 16 | (uint16_t)(r + 1);
}

// ---- CODE_B5F5E1: a level object's tiles into OAM, through the WRAM port ----
//
// For the object at X in $7FA630's table: which of its tile rows the
// screen shows, then for each set bit of each row's mask, a sprite: four
// bytes written through $2180 (WRAM's port, at OAM's address) and two bits
// of the high table gathered in $4A, stored every fourth sprite. Returns
// with carry set when done, or with $70 = $0400 when OAM is full.
//
// Unlike the recompiled code (whose width there comes from the
// disassembly), the bytes at $B5F74B run as LDA #$80; STA $4A, as on the
// SNES (with A 8-bit, as at $B5F6E9).

static inline uint16_t adc16v(uint16_t a, uint16_t b, uint8_t *c, uint8_t *v) {
  uint32_t r = (uint32_t)a + b + *c;
  *v = ((~(a ^ b) & (a ^ r)) >> 15) & 1;
  *c = (r >> 16) & 1;
  return (uint16_t)r;
}
static inline uint16_t sbc16v(uint16_t a, uint16_t b, uint8_t *c, uint8_t *v) {
  return adc16v(a, (uint16_t)~b, c, v);
}
static inline uint8_t adc8v(uint8_t a, uint8_t b, uint8_t *c, uint8_t *v) {
  uint32_t r = (uint32_t)a + b + *c;
  *v = ((~(a ^ b) & (a ^ r)) >> 7) & 1;
  *c = (r >> 8) & 1;
  return (uint8_t)r;
}

static uint32_t b5f5e1_rts(uint16_t a, uint16_t x, uint16_t y, uint8_t c, uint8_t v, uint8_t n, uint8_t z) {
  cpu.a = a;
  cpu.x = x;
  cpu.y = y;
  cpu.c = c;
  cpu.v = v;
  cpu.n = n;
  cpu.z = z;
  cpu.mf = 0;
  cpu.xf = 0;
  uint16_t r = pull16();
  return (uint32_t)cpu.pb << 16 | (uint16_t)(r + 1);
}

uint32_t R_b5f5e1(uint32_t entry) {
  if (entry != 0xb5f5e1 || cpu.d != 0 || cpu.e || cpu.mf || cpu.xf || cpu.db != 0xb5)
    return R_b5f5e1_recomp(entry);
  uint16_t x = cpu.x, y, a;
  uint8_t c = 1, v = cpu.v;
  // Its rows' mask, by its height on screen
  a = sbc16v(rd16(0x7fa634 + x), b16(0x17c0), &c, &v);
  s16(0x3e, a);
  if (!c) {
    a = adc16v(a, 0x008f, &c, &v);
    y = (uint16_t)((a >> 3) << 1);
    a = b16(0xfaa2 + y);
  } else {
    y = 0;
    a = sbc16v(a, 0x0060, &c, &v);
    if (c) y = (uint16_t)((a >> 3) << 1);
    a = b16(0xfac4 + y);
  }
  s16(0x40, a);
  // Its columns, by its place across the screen
  c = 1;
  a = sbc16v(rd16(0x7fa632 + x), b16(0x17ba), &c, &v);
  s16(0x3c, a);
  bool clipped = c;
  if (!clipped) {
    a = adc16v(a, 0x000f, &c, &v);
    if (c) clipped = true;
  }
  if (!clipped) {
    a = adc16v(a, rd16(0x7fa638 + x), &c, &v);
    a = (uint16_t)((a >> 3) << 1);
    s16(0x32, a);
    c = 1;
    a = sbc16v(rd16(0x7fa636 + x), d16(0x32), &c, &v);
    y = a;
    c = 0;
    a = adc16v(a, rd16(0x7fa63c + x), &c, &v);
    s16(0x39, a);
    c = (y >> 14) & 1;   // TYA; ASL; ASL
    a = adc16v((uint16_t)(y << 2), d16(0x3c), &c, &v);
    s16(0x3c, a);
  } else {
    c = 1;
    a = sbc16v(0x0107, d16(0x3c), &c, &v);
    a = (uint16_t)((a >> 3) << 1);
    uint16_t w = rd16(0x7fa636 + x);
    c = a >= w;   // CMP
    if (c) a = w;
    s16(0x32, a);
    a = rd16(0x7fa63c + x);
    s16(0x39, a);
  }
  // Its palette row, from the frame counter and its place
  {
    uint16_t t = (uint16_t)((d16(0x2a) >> 1) << 1);
    c = (t >> 15) & 1;
    a = adc16v((uint16_t)(t << 1), rd16(0x7fa632 + x), &c, &v);
    c = 0;
    a = adc16v(a, rd16(0x7fa634 + x), &c, &v);
    c = (a >> 1) & 1;
    a >>= 2;
    c = 0;
    a = adc16v(a, d16(0x39), &c, &v);
    c = 1;
    a = sbc16v(a, rd16(0x7fa63c + x), &c, &v);
    a = (uint16_t)((a & 0x000e) | b16(0x0d2c));
    s16(0x42, a);
  }
  // SEP #$10: X and Y 8-bit
  x &= 0xff;
  y = d8(0x32);
  s16(0x3e, (uint16_t)(d16(0x3e) - 1));
  uint32_t p39 = ptr24(0x39);
  for (;;) {
    // CODE_B5F687
    y = (uint8_t)(y - 2);
    if (y & 0x80) return b5f5e1_rts(a, x, y, 1, v, 1, 0);   // CODE_B5F67D: done
    a = (uint16_t)(ind16(p39, y) & d16(0x40));
    if (!a) continue;
    a = (uint16_t)(a >> 8 | a << 8);
    push16(a);
    x = pull8();
    // Two passes: the row's mask's first byte, then its second
    for (int pass = x ? 0 : 1; pass < 2; pass++) {
      if (pass == 1) {
        // CODE_B5F6F6
        x = pull8();
        if (!x) break;
      }
      uint32_t tbl = pass ? 0xb5fda8 : 0xb5fca8;
      do {
        // CODE_B5F696 / CODE_B5F6F9
        c = 0;
        uint16_t w = adc16v(d16(0x3c), b16(0xfb66 + y), &c, &v);
        c = 1;   // SEP #$21
        wr8(0xb52180, (uint8_t)w);
        wr8(0xb52180, adc8v(d8(0x3e), rd8(tbl + x), &c, &v));
        uint8_t t = rd8(tbl + x);
        c = (t >> 1) & 1;
        t >>= 2;
        t = adc8v(t, rd8(0xb5fb26 + y), &c, &v);
        t &= 0x0e;
        t = adc8v(t, d8(0x42), &c, &v);
        t &= 0xee;
        wr8(0xb52180, t);
        wr8(0xb52180, d8(0x43));
        // XBA; ASL: the X position's ninth bit; into $4A with a size bit
        c = (w >> 15) & 1;
        uint8_t q = d8(0x4a);
        uint8_t out = q & 1;
        q = (uint8_t)(q >> 1 | c << 7);
        c = out;
        out = q & 1;
        q = (uint8_t)(q >> 1 | 0x80);
        c = out;
        a = (uint16_t)((uint16_t)d8(0x43) << 8 | q);
        if (c) {
          // A byte of the high table full: stored, every fourth sprite
          push8((uint8_t)x);
          uint8_t k = d8(0x38);
          wr8(0xb503a0 + k, q);
          uint8_t k1 = (uint8_t)(k + 1);
          s8(0x38, k1);
          if (k1 & 0x80) {
            // OAM is full
            if (pass == 0) {
              pull16();   // CODE_B5F6D5: PLA (the byte left by PHA/PLX and X)
            } else {
              k = (uint8_t)pull8();   // CODE_B5F739: PLX
            }
            s16(0x70, 0x0400);
            return b5f5e1_rts(0x0400, k, y, 1, v, 0, 0);   // X: $38 before the INC, or the PLX'd X
          }
          s8(0x4a, 0x80);
          x = pull8();
        } else {
          s8(0x4a, q);
        }
        a = rd16(0xb5fba8 + x);
        x = a & 0xff;
      } while (x);
    }
  }
}

// ---- decompress_data ($BB8D91) and decompress_data_default ($BB8D9E) ----
//
// The game's decompressor: a stream of nibbles, each command in the high
// nibble of a byte (set 1) or the low one (set 2), the commands' operands
// in the nibbles that follow. Native here command by command, as the 65816
// code does to memory: the output, the direct page's variables, the nibble
// tables it builds at $7E2000, and the stack's bytes (the PEIs that
// dispatch the commands, the PHYs, the lookup builder's JSL), done with
// real pushes and pulls. Every command is entered with carry clear, so the
// ADCs without a CLC are plain adds.

enum { SET1, SET2, SET2_WIDE, DONE };

uint32_t R_bb8000(uint32_t entry) {
  if ((entry != 0xbb8d91 && entry != 0xbb8d9e) || cpu.d != 0 || cpu.e || cpu.mf || cpu.xf)
    return R_bb8000_recomp(entry);
  uint16_t a = cpu.a, x = cpu.x, y = cpu.y;
  if (entry == 0xbb8d91) {
    s16(0x38, a);
    s16(0x3a, 0x007f);
    s16(0x34, x);
    s16(0x36, y);
  } else {
    s16(0x34, x);
    s16(0x36, y);
    x = y;
    s16(0x38, 0x0000);
    s16(0x3a, 0x007f);
  }
  // start_decompression
  s16(0x32, 0xffff);
  push8(cpu.db);
  push16(y);
  s16(0x46, 0x2100);
  s16(0x48, 0x007e);
  s16(0x44, 0x007e);
  s16(0x42, 0x2000);
  // JSL build_decompression_lookup: the nibble tables
  push8(0xbb);
  push16(0x8dc9);
  for (int i = 0; i < 256; i++) {
    wr8(0x7e2100 + i, (uint8_t)((i & 0x0f) << 4));
    wr8(0x7e2000 + i, (uint8_t)(i >> 4));
  }
  pull16();
  pull8();
  x = d16(0x38);
  y = 1;
  s16(0x4e, 0x8001);
  s16(0x4a, 0x8040);
  // DB = the destination's bank (PHA; PLB)
  uint8_t dbank = d8(0x3a);
  push8(dbank);
  pull8();
  uint32_t db = (uint32_t)dbank << 16;
  uint32_t src = ptr24(0x34);
  #define SRC() rd8((src + y++) & 0xffffff)
  #define OUT(v) wr8((db + x++) & 0xffffff, (uint8_t)(v))
  #define LO(v) ((uint8_t)((v) >> 4))          /* [$42]: the high nibble, as a value */
  #define HI(v) ((uint8_t)(((v) & 0x0f) << 4)) /* [$46]: the low nibble, moved up */
  s8(0x52, SRC());
  s8(0x53, SRC());
  s8(0x56, SRC());
  s8(0x57, SRC());
  s16(0x54, (uint16_t)(ind8(src, y) | ind8(src, (uint16_t)(y + 1)) << 8));
  y = 0x27;
  s16(0x3c, 0);
  s16(0x3e, 0);
  int next = SET1;
  for (;;) {
    int cmd;
    // The dispatch: PEI of the command's address, RTS
    if (next == SET1) {
      uint8_t b = SRC();
      s8(0x46, b);
      s8(0x4e, (uint8_t)((b & 0xf0) >> 2));
      push16(d16(0x4e));
      pull16();
      cmd = b >> 4;
    } else {
      uint8_t n = (next == SET2 ? d8(0x42) : d8(0x46)) & 0x0f;
      s8(0x4a, (uint8_t)((n << 2) + 0x3f));
      push16(d16(0x4a));
      pull16();
      cmd = 16 + n;
    }
    uint8_t op = d8(0x46);
    switch (cmd) {
      case 0: {   // copy_or_return_1
        uint8_t n = op & 0x0f;
        if (!n) { next = DONE; break; }
        s8(0x3c, n);
        do { OUT(SRC()); s8(0x3c, (uint8_t)(d8(0x3c) - 1)); } while (d8(0x3c));
        next = SET1;
        break;
      }
      case 16: {  // copy_or_return_2
        uint8_t b = SRC();
        s8(0x42, b);
        s8(0x46, b);
        if (!LO(b)) { next = DONE; break; }
        s8(0x3c, LO(b));
        do {
          s8(0x3e, HI(d8(0x46)));
          b = SRC();
          s8(0x42, b);
          s8(0x46, b);
          OUT(d8(0x3e) | LO(b));
          s8(0x3c, (uint8_t)(d8(0x3c) - 1));
        } while (d8(0x3c));
        next = SET2_WIDE;
        break;
      }
      case 1: {   // stream_byte_1
        s8(0x3e, HI(op));
        uint8_t b = SRC();
        s8(0x42, b);
        OUT(d8(0x3e) | LO(b));
        next = SET2;
        break;
      }
      case 17:    // stream_byte_2
        OUT(SRC());
        next = SET1;
        break;
      case 2: {   // stream_word_1
        s8(0x3e, HI(op));
        uint8_t b = SRC();
        s8(0x42, b);
        s8(0x46, b);
        OUT(d8(0x3e) | LO(b));
        s8(0x3e, HI(b));
        b = SRC();
        s8(0x42, b);
        OUT(d8(0x3e) | LO(b));
        next = SET2;
        break;
      }
      case 18: {  // stream_word_2
        uint16_t w = (uint16_t)(ind8(src, y) | ind8(src, (uint16_t)(y + 1)) << 8);
        wr16((db + x) & 0xffffff, w);
        y += 2;
        x += 2;
        next = SET1;
        break;
      }
      case 3: case 4: case 5: {   // stream_byte_fill_1, direct_byte_1/2_fill_1
        uint8_t n = (uint8_t)((op & 0x0f) + 3);
        s8(0x3c, n);
        uint8_t v = cmd == 3 ? SRC() : d8(cmd == 4 ? 0x52 : 0x53);
        do { OUT(v); s8(0x3c, (uint8_t)(d8(0x3c) - 1)); } while (d8(0x3c));
        next = SET1;
        break;
      }
      case 19: {  // stream_byte_fill_2
        uint8_t b = SRC();
        s8(0x42, b);
        s8(0x46, b);
        s8(0x3c, (uint8_t)(LO(b) + 3));
        s8(0x3e, HI(b));
        b = SRC();
        s8(0x42, b);
        uint8_t v = d8(0x3e) | LO(b);
        do { OUT(v); s8(0x3c, (uint8_t)(d8(0x3c) - 1)); } while (d8(0x3c));
        next = SET2;
        break;
      }
      case 20: case 21: {   // direct_byte_1/2_fill_2
        uint8_t b = SRC();
        s8(0x42, b);
        s8(0x3c, (uint8_t)(LO(b) + 3));
        uint8_t v = d8(cmd == 20 ? 0x52 : 0x53);
        do { OUT(v); s8(0x3c, (uint8_t)(d8(0x3c) - 1)); } while (d8(0x3c));
        next = SET2;
        break;
      }
      case 6: case 22:   // direct_word_put_1/2
        wr16((db + x) & 0xffffff, d16(0x54));
        x += 2;
        next = cmd == 6 ? SET2_WIDE : SET1;
        break;
      case 7: case 23:   // direct_byte_1_put_1/2
        OUT(d8(0x56));
        next = cmd == 7 ? SET2_WIDE : SET1;
        break;
      case 8: case 24:   // direct_byte_2_put_1/2
        OUT(d8(0x57));
        next = cmd == 8 ? SET2_WIDE : SET1;
        break;
      case 9: case 25: {   // back_copy_word_1/2
        if (cmd == 9) {
          push16(y);
          s8(0x3e, (uint8_t)((op & 0x0f) + 2));
        } else {
          uint8_t b = SRC();
          s8(0x42, b);
          s8(0x3e, (uint8_t)(LO(b) + 2));
          push16(y);
        }
        uint16_t from = (uint16_t)(x - d16(0x3e));
        wr16((db + x) & 0xffffff, rd16((db + from) & 0xffffff));
        x += 2;
        y = pull16();
        next = cmd == 9 ? SET1 : SET2;
        break;
      }
      case 10: case 26: case 11: case 27: case 12: case 28: {   // back copies
        uint16_t from;
        int then;
        if (cmd == 10) {   // back_copy_1
          s8(0x3c, (uint8_t)((op & 0x0f) + 3));
          s8(0x3e, SRC());
          push16(y);
          from = (uint16_t)(x - d16(0x3c) - d16(0x3e));
          then = SET1;
        } else if (cmd == 26) {   // back_copy_2
          uint8_t b = SRC();
          s8(0x42, b);
          s8(0x46, b);
          s8(0x3c, (uint8_t)(LO(b) + 3));
          s8(0x3e, HI(b));
          b = SRC();
          s8(0x42, b);
          s8(0x3e, LO(b) | d8(0x3e));
          push16(y);
          from = (uint16_t)(x - d16(0x3c) - d16(0x3e));
          then = SET2;
        } else if (cmd == 11) {   // back_copy_far_1
          s8(0x3c, (uint8_t)((op & 0x0f) + 3));
          uint8_t hi = SRC(), lo = SRC();
          s8(0x46, lo);
          s16(0x40, (uint16_t)(((hi << 8 | lo) >> 4) + 0x0103));
          push16(y);
          from = (uint16_t)(x - d16(0x40));
          then = SET2_WIDE;
        } else if (cmd == 27) {   // back_copy_far_2
          uint8_t b = SRC();
          s8(0x42, b);
          s8(0x3c, (uint8_t)(LO(b) + 3));
          uint8_t lo = SRC();
          s16(0x40, (uint16_t)(((b & 0x0f) << 8 | lo) + 0x0103));
          push16(y);
          from = (uint16_t)(x - d16(0x40));
          then = SET1;
        } else if (cmd == 12) {   // back_copy_arbitrary_1
          s8(0x3c, (uint8_t)((op & 0x0f) + 3));
          uint16_t w = (uint16_t)(ind8(src, y) | ind8(src, (uint16_t)(y + 1)) << 8);
          s16(0x40, (uint16_t)(w >> 8 | w << 8));
          y += 2;
          push16(y);
          from = (uint16_t)(x - d16(0x40));
          then = SET1;
        } else {   // back_copy_arbitrary_2
          uint8_t b = SRC();
          s8(0x42, b);
          s8(0x3c, (uint8_t)(LO(b) + 3));
          uint8_t lo = SRC();
          s16(0x40, (uint16_t)((b << 8 | lo) << 4));
          uint8_t b2 = SRC();
          push16(y);
          s8(0x42, b2);
          s16(0x40, (uint16_t)(LO(b2) | d16(0x40)));
          from = (uint16_t)(x - d16(0x40));
          then = SET2;
        }
        do {
          OUT(rd8((db + from++) & 0xffffff));
          s8(0x3c, (uint8_t)(d8(0x3c) - 1));
        } while (d8(0x3c));
        y = pull16();
        next = then;
        break;
      }
      case 13: case 29:   // duplicate_byte_1/2
        wr8((db + (uint16_t)(x - 1) + 1) & 0xffffff, rd8((db + (uint16_t)(x - 1)) & 0xffffff));
        x += 1;
        next = cmd == 13 ? SET2_WIDE : SET1;
        break;
      case 14: case 30:   // duplicate_word_1/2
        wr16((db + (uint16_t)(x - 2) + 2) & 0xffffff, rd16((db + (uint16_t)(x - 2)) & 0xffffff));
        x += 2;
        next = cmd == 14 ? SET2_WIDE : SET1;
        break;
      case 15: {  // copy_future_word_1: a word of the header
        push16(y);
        uint16_t at = (uint16_t)(((op & 0x0f) << 1) + 7);
        wr16((db + x) & 0xffffff, (uint16_t)(ind8(src, at) | ind8(src, (uint16_t)(at + 1)) << 8));
        x += 2;
        y = pull16();
        next = SET1;
        break;
      }
      case 31: {  // copy_future_word_2
        uint8_t b = SRC();
        s8(0x46, b);
        push16(y);
        uint16_t at = (uint16_t)(((b & 0xf0) >> 3) + 7);
        wr16((db + x) & 0xffffff, (uint16_t)(ind8(src, at) | ind8(src, (uint16_t)(at + 1)) << 8));
        x += 2;
        y = pull16();
        next = SET2_WIDE;
        break;
      }
    }
    if (next == DONE) break;
  }
  #undef SRC
  #undef OUT
  #undef LO
  #undef HI
  // finalize_decompression: REP #$20; PLY; PLB; X = the length; LDA $32 ($FFFF: no DMA); RTL
  y = pull16();
  cpu.db = pull8();
  uint8_t c = 1, v = 0;
  x = sbc16v(x, d16(0x38), &c, &v);
  cpu.a = d16(0x32);
  cpu.x = x;
  cpu.y = y;
  cpu.c = c;
  cpu.v = v;
  cpu.n = (cpu.a >> 15) & 1;
  cpu.z = cpu.a == 0;
  cpu.mf = 0;
  uint16_t r = pull16();
  uint32_t b = pull8();
  return b << 16 | (uint16_t)(r + 1);
}
