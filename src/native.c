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
