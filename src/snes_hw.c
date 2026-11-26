// See snes_hw.h. The ports behave as LakeSnes's (third_party/lakesnes/ppu.c,
// snes.c, dma.c, cart.c), which is what the game is checked against.
#include "cpu65.h"   // first: the platform's HOT_CODE
#include "snes_hw.h"

#include <string.h>

#include "snes_mem.h"

#ifndef EWRAM_DATA
#define EWRAM_DATA
#endif

EWRAM_DATA SnesHw g_hw;
EWRAM_DATA __attribute__((aligned(4))) uint8_t g_wram[0x20000];
EWRAM_DATA uint8_t g_sram[0x800];
const uint8_t *g_rom;
uint32_t g_rom_mask;

void hw_reset(void) {
  memset(&g_hw, 0, sizeof(g_hw));
  memset(g_wram, 0, sizeof(g_wram));
  g_hw.vram_increment = 1;
  g_hw.multiply_a = 0xff;
  g_hw.multiply_result = 0xfe01;
  g_hw.divide_a = 0xffff;
  g_hw.divide_result = 0x101;
  // What the APU's IPL ROM shows when it's ready for an upload.
  g_hw.apu_out[0] = 0xaa;
  g_hw.apu_out[1] = 0xbb;
}

// Buttons in the order the serial port shifts them out: B first.
static uint16_t serial_order(uint16_t joy) {
  uint16_t s = 0;
  for (int i = 0; i < 16; i++)
    if (joy & (0x8000 >> i)) s |= 1 << i;
  return s;
}

void hw_vblank(uint16_t joy1) {
  g_hw.in_nmi = 1;
  if (!(g_hw.regs[0x00] & 0x80)) {
    // Unless the screen is blanked, the OAM address goes back to the one
    // last written: the game's OAM DMA counts on it.
    g_hw.oam_adr = g_hw.oam_adr_written;
    g_hw.oam_in_high = g_hw.oam_in_high_written;
    g_hw.oam_second_write = 0;
  }
  g_hw.joy1_now = joy1;
  if (g_hw.nmitimen & 1) {
    // Auto-joypad: latch, shift 16 bits out of each port (ones shift in).
    g_hw.joy[0] = joy1;
    g_hw.joy[1] = 0;
    g_hw.serial[0] = g_hw.serial[1] = 0xffff;
  }
}

static uint8_t serial_read(int port) {
  if (g_hw.strobe) g_hw.serial[port] = port ? 0 : serial_order(g_hw.joy1_now);
  uint8_t bit = g_hw.serial[port] & 1;
  g_hw.serial[port] = g_hw.serial[port] >> 1 | 0x8000;
  return bit;
}

// ---- PPU ports ----

static uint16_t vram_remap(void) {
  uint16_t adr = g_hw.vram_pointer;
  switch (g_hw.vram_remap) {
    case 0: return adr;
    case 1: return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
    case 2: return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
    default: return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
  }
}

void (*g_ppu_write_hook)(uint8_t reg, uint8_t value);

HOT_CODE static void ppu_write(uint8_t adr, uint8_t val) {
  g_hw.regs[adr] = val;
  if (g_ppu_write_hook) g_ppu_write_hook(adr, val);
  switch (adr) {
    case 0x0d: case 0x0f: case 0x11: case 0x13:
      g_hw.hofs[(adr - 0x0d) >> 1] =
          (uint16_t)((val << 8 | (g_hw.scroll_prev & 0xf8) | (g_hw.scroll_prev2 & 7)) & 0x3ff);
      g_hw.scroll_prev = g_hw.scroll_prev2 = val;
      if (adr == 0x0d) {
        g_hw.m7matrix[6] = (int16_t)((val << 8 | g_hw.m7prev) & 0x1fff);
        g_hw.m7prev = val;
      }
      break;
    case 0x0e: case 0x10: case 0x12: case 0x14:
      g_hw.vofs[(adr - 0x0e) >> 1] = (uint16_t)((val << 8 | g_hw.scroll_prev) & 0x3ff);
      g_hw.scroll_prev = val;
      if (adr == 0x0e) {
        g_hw.m7matrix[7] = (int16_t)((val << 8 | g_hw.m7prev) & 0x1fff);
        g_hw.m7prev = val;
      }
      break;
    case 0x32:
      if (val & 0x80) g_hw.fixed_b = val & 0x1f;
      if (val & 0x40) g_hw.fixed_g = val & 0x1f;
      if (val & 0x20) g_hw.fixed_r = val & 0x1f;
      break;
    case 0x02:
      g_hw.oam_adr = g_hw.oam_adr_written = val;
      g_hw.oam_in_high = g_hw.oam_in_high_written;
      g_hw.oam_second_write = 0;
      break;
    case 0x03:
      g_hw.oam_in_high = g_hw.oam_in_high_written = val & 1;
      g_hw.oam_adr = g_hw.oam_adr_written;
      g_hw.oam_second_write = 0;
      break;
    case 0x04:
      if (g_hw.oam_in_high) {
        g_hw.high_oam[((g_hw.oam_adr & 0xf) << 1) | g_hw.oam_second_write] = val;
        if (g_hw.oam_second_write) {
          g_hw.oam_adr++;
          if (g_hw.oam_adr == 0) g_hw.oam_in_high = 0;
        }
      } else if (!g_hw.oam_second_write) {
        g_hw.oam_buffer = val;
      } else {
        g_hw.oam[g_hw.oam_adr++] = (uint16_t)(val << 8 | g_hw.oam_buffer);
        if (g_hw.oam_adr == 0) g_hw.oam_in_high = 1;
      }
      g_hw.oam_second_write ^= 1;
      break;
    case 0x15:
      g_hw.vram_increment = (val & 3) == 0 ? 1 : (val & 3) == 1 ? 32 : 128;
      g_hw.vram_remap = (val & 0xc) >> 2;
      g_hw.vram_increment_on_high = val >> 7;
      break;
    case 0x16:
      g_hw.vram_pointer = (g_hw.vram_pointer & 0xff00) | val;
      g_hw.vram_read_buffer = g_hw.vram[vram_remap() & 0x7fff];
      break;
    case 0x17:
      g_hw.vram_pointer = (uint16_t)((g_hw.vram_pointer & 0x00ff) | val << 8);
      g_hw.vram_read_buffer = g_hw.vram[vram_remap() & 0x7fff];
      break;
    case 0x18: {
      uint16_t a = vram_remap() & 0x7fff;
      g_hw.vram[a] = (g_hw.vram[a] & 0xff00) | val;
      g_hw.vram_dirty[a >> 8] |= 1u << ((a >> 3) & 31);
      if (!g_hw.vram_increment_on_high) g_hw.vram_pointer += g_hw.vram_increment;
      break;
    }
    case 0x19: {
      uint16_t a = vram_remap() & 0x7fff;
      g_hw.vram[a] = (uint16_t)((g_hw.vram[a] & 0x00ff) | val << 8);
      g_hw.vram_dirty[a >> 8] |= 1u << ((a >> 3) & 31);
      if (g_hw.vram_increment_on_high) g_hw.vram_pointer += g_hw.vram_increment;
      break;
    }
    case 0x1b: case 0x1c: case 0x1d: case 0x1e:
      g_hw.m7matrix[adr - 0x1b] = (int16_t)(val << 8 | g_hw.m7prev);
      g_hw.m7prev = val;
      break;
    case 0x1f: case 0x20:
      g_hw.m7matrix[adr - 0x1b] = (int16_t)((val << 8 | g_hw.m7prev) & 0x1fff);
      g_hw.m7prev = val;
      break;
    case 0x21:
      g_hw.cgram_pointer = val;
      g_hw.cgram_second_write = 0;
      break;
    case 0x22:
      if (!g_hw.cgram_second_write) g_hw.cgram_buffer = val;
      else g_hw.cgram[g_hw.cgram_pointer++] = (uint16_t)(val << 8 | g_hw.cgram_buffer);
      g_hw.cgram_second_write ^= 1;
      g_hw.cgram_ver++;
      break;
    default:
      break;
  }
}

static uint8_t ppu_read(uint8_t adr) {
  switch (adr) {
    case 0x34: case 0x35: case 0x36: {
      int32_t r = (int32_t)g_hw.m7matrix[0] * (int8_t)(g_hw.m7matrix[1] >> 8);
      return (uint8_t)(r >> (8 * (adr - 0x34)));
    }
    case 0x38: {
      uint8_t ret;
      if (g_hw.oam_in_high) {
        ret = g_hw.high_oam[((g_hw.oam_adr & 0xf) << 1) | g_hw.oam_second_write];
        if (g_hw.oam_second_write) {
          g_hw.oam_adr++;
          if (g_hw.oam_adr == 0) g_hw.oam_in_high = 0;
        }
      } else if (!g_hw.oam_second_write) {
        ret = g_hw.oam[g_hw.oam_adr] & 0xff;
      } else {
        ret = g_hw.oam[g_hw.oam_adr++] >> 8;
        if (g_hw.oam_adr == 0) g_hw.oam_in_high = 1;
      }
      g_hw.oam_second_write ^= 1;
      return ret;
    }
    case 0x39: case 0x3a: {
      uint16_t v = g_hw.vram_read_buffer;
      if ((adr == 0x3a) == (g_hw.vram_increment_on_high != 0)) {
        g_hw.vram_read_buffer = g_hw.vram[vram_remap() & 0x7fff];
        g_hw.vram_pointer += g_hw.vram_increment;
      }
      return adr == 0x39 ? (uint8_t)v : v >> 8;
    }
    case 0x3b: {
      uint8_t ret;
      if (!g_hw.cgram_second_write) ret = g_hw.cgram[g_hw.cgram_pointer] & 0xff;
      else ret = (g_hw.cgram[g_hw.cgram_pointer++] >> 8) & 0x7f;
      g_hw.cgram_second_write ^= 1;
      return ret;
    }
    case 0x3e: return 0x01;   // PPU1 version
    case 0x3f: return 0x01;   // PPU2 version, NTSC
    default: return g_hw.open_bus;
  }
}

// ---- the APU's port 0 (see snes_hw.h) ----

enum { APU_IPL_READY, APU_IPL_TRANSFER, APU_DRIVER_STARTING, APU_DRIVER };

// The SPC700 sees a write to port 0 some time after the CPU makes it, after
// the rest of a 16-bit write too, so a write is acted on at the next read.
static void apu_update(void) {
  if (!g_hw.apu_pending) {
    if (g_hw.apu_state == APU_DRIVER_STARTING) {
      // The driver's first act is to clear port 0.
      g_hw.apu_out[0] = 0;
      g_hw.apu_state = APU_DRIVER;
    }
    return;
  }
  g_hw.apu_pending = 0;
  uint8_t v = g_hw.apu_in[0];
  switch (g_hw.apu_state) {
    case APU_IPL_READY:
      if (v == 0xcc) {
        g_hw.apu_out[0] = 0xcc;
        g_hw.apu_state = APU_IPL_TRANSFER;
        g_hw.apu_counter = 0;
      }
      break;
    case APU_IPL_TRANSFER:
      g_hw.apu_out[0] = v;
      if (v == g_hw.apu_counter) {
        g_hw.apu_counter++;        // a data byte
      } else if (g_hw.apu_in[1] == 0) {
        g_hw.apu_state = APU_DRIVER_STARTING;   // jump to the uploaded code
      } else {
        g_hw.apu_counter = 0;      // the next block
      }
      break;
    default:
      g_hw.apu_out[0] = v;
      g_hw.apu_state = APU_DRIVER;
      break;
  }
}

// ---- the B bus: PPU, APU ports, WRAM port ----

HOT_CODE static void bbus_write(uint8_t adr, uint8_t val) {
  if (adr < 0x40) {
    ppu_write(adr, val);
  } else if (adr < 0x80) {
    g_hw.apu_in[adr & 3] = val;
    if ((adr & 3) == 0) g_hw.apu_pending = 1;
  } else if (adr == 0x80) {
    g_wram[g_hw.wram_adr] = val;
    g_hw.wram_adr = (g_hw.wram_adr + 1) & 0x1ffff;
  } else if (adr == 0x81) {
    g_hw.wram_adr = (g_hw.wram_adr & 0x1ff00) | val;
  } else if (adr == 0x82) {
    g_hw.wram_adr = (g_hw.wram_adr & 0x100ff) | val << 8;
  } else if (adr == 0x83) {
    g_hw.wram_adr = (g_hw.wram_adr & 0x0ffff) | (uint32_t)(val & 1) << 16;
  }
}

static uint8_t bbus_read(uint8_t adr) {
  if (adr < 0x40) return ppu_read(adr);
  if (adr < 0x80) {
    apu_update();
    return g_hw.apu_out[adr & 3];
  }
  if (adr == 0x80) {
    uint8_t v = g_wram[g_hw.wram_adr];
    g_hw.wram_adr = (g_hw.wram_adr + 1) & 0x1ffff;
    return v;
  }
  return g_hw.open_bus;
}

// ---- DMA ----

static const uint8_t kBAdrOffsets[8][4] = {
  {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
  {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
};

// A host pointer to `len` bytes of the A bus from `addr` up, if they're all
// plain memory (WRAM or ROM) in one piece; else NULL.
static const uint8_t *direct_source(uint32_t addr, uint32_t len) {
  uint8_t bank = addr >> 16;
  uint32_t lo = addr & 0xffff;
  if (lo + len > 0x10000) return 0;
  if ((bank & 0xfe) == 0x7e) return &g_wram[addr & 0x1ffff];
  if ((bank & 0x7f) < 0x40) {
    if (lo + len <= 0x2000) return &g_wram[lo];
    if (lo >= 0x8000) return &g_rom[(((uint32_t)(bank & 0x3f) << 16) | lo) & g_rom_mask];
    return 0;
  }
  return &g_rom[(((uint32_t)(bank & 0x3f) << 16) | lo) & g_rom_mask];
}

// The common big transfer: words to VRAM ($2118/$2119, mode 1), the
// address stepping after the high byte, no remapping. As the byte-by-byte
// path would do it, write for write.
HOT_CODE static bool fast_vram_dma(const uint8_t *src, bool fixed, uint32_t size) {
  if (!g_hw.vram_increment_on_high || g_hw.vram_remap || (size & 1)) return false;
  uint16_t ptr = g_hw.vram_pointer, inc = g_hw.vram_increment;
  uint16_t *vram = g_hw.vram;
  uint32_t *dirty = g_hw.vram_dirty;
  if (fixed) {
    // A fixed source writes its first byte to both halves.
    uint16_t w = (uint16_t)(src[0] | src[0] << 8);
    for (uint32_t n = size >> 1; n; n--) {
      uint16_t a = ptr & 0x7fff;
      vram[a] = w;
      dirty[a >> 8] |= 1u << ((a >> 3) & 31);
      ptr += inc;
    }
  } else {
    uint32_t last_chunk = 0xffffffff;
    for (uint32_t n = size >> 1; n; n--, src += 2) {
      uint32_t a = ptr & 0x7fff;
      vram[a] = (uint16_t)(src[0] | src[1] << 8);
      if (a >> 3 != last_chunk) {
        last_chunk = a >> 3;
        dirty[a >> 8] |= 1u << ((a >> 3) & 31);
      }
      ptr += inc;
    }
  }
  g_hw.vram_pointer = ptr;
  g_hw.regs[0x18] = src[fixed ? 0 : -2];
  g_hw.regs[0x19] = src[fixed ? 0 : -1];
  return true;
}

// OAM ($2104) and CGRAM ($2122) uploads, mode 0: the ports' own steps
// (ppu_write), without the per-byte dispatch.
HOT_CODE static void fast_oam_dma(const uint8_t *src, uint32_t size) {
  if (!size) return;
  // The port's state in locals for the run (g_hw is in the GBA's EWRAM).
  uint8_t adr = g_hw.oam_adr, high = g_hw.oam_in_high, second = g_hw.oam_second_write, buffer = g_hw.oam_buffer;
  uint8_t val = 0;
  for (; size; size--) {
    if (!high && !second && size >= 2) {
      // A whole word of the main table
      buffer = src[0];
      val = src[1];
      src += 2;
      size--;
      g_hw.oam[adr++] = (uint16_t)(val << 8 | buffer);
      if (adr == 0) high = 1;
      continue;
    }
    val = *src++;
    if (high) {
      g_hw.high_oam[((adr & 0xf) << 1) | second] = val;
      if (second) {
        adr++;
        if (adr == 0) high = 0;
      }
    } else if (!second) {
      buffer = val;
    } else {
      g_hw.oam[adr++] = (uint16_t)(val << 8 | buffer);
      if (adr == 0) high = 1;
    }
    second ^= 1;
  }
  g_hw.oam_adr = adr;
  g_hw.oam_in_high = high;
  g_hw.oam_second_write = second;
  g_hw.oam_buffer = buffer;
  g_hw.regs[0x04] = val;
}

HOT_CODE static void fast_cgram_dma(const uint8_t *src, uint32_t size) {
  if (!size) return;
  uint8_t ptr = g_hw.cgram_pointer, second = g_hw.cgram_second_write, buffer = g_hw.cgram_buffer, val = 0;
  for (uint32_t n = size; n; n--) {
    val = *src++;
    if (!second) buffer = val;
    else g_hw.cgram[ptr++] = (uint16_t)(val << 8 | buffer);
    second ^= 1;
  }
  g_hw.cgram_pointer = ptr;
  g_hw.cgram_second_write = second;
  g_hw.cgram_buffer = buffer;
  g_hw.regs[0x22] = val;
  g_hw.cgram_ver = (uint16_t)(g_hw.cgram_ver + size);
}

static void do_dma(uint8_t channels) {
  for (int c = 0; c < 8; c++) {
    if (!(channels & (1 << c))) continue;
    uint8_t *r = g_hw.dma[c];
    uint8_t mode = r[0] & 7, b_adr = r[1];
    bool from_b = r[0] & 0x80, fixed = r[0] & 0x08, decrement = r[0] & 0x10;
    uint16_t a_adr = r[2] | r[3] << 8;
    uint32_t a_bank = (uint32_t)r[4] << 16;
    uint32_t size = r[5] | r[6] << 8;
    if (size == 0) size = 0x10000;
    if (!from_b && !decrement && !g_ppu_write_hook) {
      if (mode == 1 && b_adr == 0x18) {
        const uint8_t *src = direct_source(a_bank | a_adr, fixed ? 1 : size);
        if (src && fast_vram_dma(src, fixed, size)) {
          if (!fixed) a_adr = (uint16_t)(a_adr + size);
          size = 0;
        }
      } else if ((mode == 0 || mode == 2) && !fixed && (b_adr == 0x04 || b_adr == 0x22)) {
        const uint8_t *src = direct_source(a_bank | a_adr, size);
        if (src) {
          if (b_adr == 0x04) fast_oam_dma(src, size);
          else fast_cgram_dma(src, size);
          a_adr = (uint16_t)(a_adr + size);
          size = 0;
        }
      }
    }
    int off = 0;
    while (size--) {
      uint8_t b = (uint8_t)(b_adr + kBAdrOffsets[mode][off++]);
      off &= 3;
      if (from_b) wr8(a_bank | a_adr, bbus_read(b));
      else bbus_write(b, rd8(a_bank | a_adr));
      if (!fixed) a_adr += decrement ? -1 : 1;
    }
    r[2] = (uint8_t)a_adr;
    r[3] = a_adr >> 8;
    r[5] = r[6] = 0;
  }
}

// ---- HDMA ----

static const uint8_t kTransferLength[8] = {1, 2, 2, 4, 4, 4, 2, 4};

void hw_hdma_begin(void) {
  for (int c = 0; c < 8; c++) {
    const uint8_t *r = g_hw.dma[c];
    g_hw.hdma[c].active = (g_hw.hdmaen >> c) & 1;
    g_hw.hdma[c].do_transfer = 0;
    if (!g_hw.hdma[c].active) continue;
    uint32_t bank = (uint32_t)r[4] << 16;
    uint16_t t = r[2] | r[3] << 8;
    g_hw.hdma[c].lines = rd8(bank | t++);
    if (g_hw.hdma[c].lines == 0) g_hw.hdma[c].active = 0;
    if (r[0] & 0x40) {
      uint16_t ind = rd8(bank | t) | rd8(bank | (uint16_t)(t + 1)) << 8;
      t += 2;
      g_hw.hdma[c].indirect = (uint32_t)r[7] << 16 | ind;
    }
    g_hw.hdma[c].table = bank | t;
    g_hw.hdma[c].do_transfer = 1;
  }
}

// The same as hw_hdma_begin and 224 hw_hdma_line calls, on a copy of the
// active channels' state in fast memory (g_hw is in the GBA's EWRAM).
bool g_hdma_wrote;

HOT_CODE void hw_hdma_frame(void (*after_line)(int line)) {
  hw_hdma_begin();
  struct {
    uint32_t table, indirect;
    uint8_t lines, do_transfer, active, c;
    uint8_t mode, badr, ibank;
    bool indirect_mode;
  } ch[8];
  int n = 0;
  for (int c = 0; c < 8; c++) {
    if (!g_hw.hdma[c].active) continue;
    ch[n].table = g_hw.hdma[c].table;
    ch[n].indirect = g_hw.hdma[c].indirect;
    ch[n].lines = g_hw.hdma[c].lines;
    ch[n].do_transfer = g_hw.hdma[c].do_transfer;
    ch[n].active = 1;
    ch[n].c = (uint8_t)c;
    ch[n].mode = g_hw.dma[c][0] & 7;
    ch[n].indirect_mode = g_hw.dma[c][0] & 0x40;
    ch[n].badr = g_hw.dma[c][1];
    ch[n].ibank = g_hw.dma[c][7];
    n++;
  }
  for (int line = 0; line < 224; line++) {
    g_hdma_wrote = false;
    for (int k = 0; k < n; k++) {
      if (!ch[k].active) continue;
      bool indirect = ch[k].indirect_mode;
      uint32_t bank = ch[k].table & 0xff0000;
      uint16_t t = (uint16_t)ch[k].table;
      if (ch[k].do_transfer) {
        uint8_t mode = ch[k].mode;
        g_hdma_wrote = true;
        for (int i = 0; i < kTransferLength[mode]; i++) {
          uint8_t v;
          if (indirect) {
            v = rd8(ch[k].indirect);
            ch[k].indirect = (ch[k].indirect & 0xff0000) | (uint16_t)(ch[k].indirect + 1);
          } else {
            v = rd8(bank | t++);
          }
          bbus_write((uint8_t)(ch[k].badr + kBAdrOffsets[mode][i]), v);
        }
      }
      ch[k].lines--;
      ch[k].do_transfer = ch[k].lines >> 7;
      if ((ch[k].lines & 0x7f) == 0) {
        ch[k].lines = rd8(bank | t++);
        if (indirect) {
          uint16_t ind = rd8(bank | t) | rd8(bank | (uint16_t)(t + 1)) << 8;
          t += 2;
          ch[k].indirect = (uint32_t)ch[k].ibank << 16 | ind;
        }
        if (ch[k].lines == 0) ch[k].active = 0;
        ch[k].do_transfer = 1;
      }
      ch[k].table = bank | t;
    }
    if (after_line) after_line(line);
  }
  for (int k = 0; k < n; k++) {
    int c = ch[k].c;
    g_hw.hdma[c].table = ch[k].table;
    g_hw.hdma[c].indirect = ch[k].indirect;
    g_hw.hdma[c].lines = ch[k].lines;
    g_hw.hdma[c].do_transfer = ch[k].do_transfer;
    g_hw.hdma[c].active = ch[k].active;
  }
}

void hw_hdma_line(void) {   // (hostmini; the GBA runs hw_hdma_frame)
  for (int c = 0; c < 8; c++) {
    if (!g_hw.hdma[c].active) continue;
    const uint8_t *r = g_hw.dma[c];
    bool indirect = r[0] & 0x40;
    uint32_t bank = g_hw.hdma[c].table & 0xff0000;
    uint16_t t = (uint16_t)g_hw.hdma[c].table;
    if (g_hw.hdma[c].do_transfer) {
      uint8_t mode = r[0] & 7;
      for (int i = 0; i < kTransferLength[mode]; i++) {
        uint8_t v;
        if (indirect) {
          v = rd8(g_hw.hdma[c].indirect);
          g_hw.hdma[c].indirect = (g_hw.hdma[c].indirect & 0xff0000) | (uint16_t)(g_hw.hdma[c].indirect + 1);
        } else {
          v = rd8(bank | t++);
        }
        bbus_write((uint8_t)(r[1] + kBAdrOffsets[mode][i]), v);
      }
    }
    g_hw.hdma[c].lines--;
    g_hw.hdma[c].do_transfer = g_hw.hdma[c].lines >> 7;
    if ((g_hw.hdma[c].lines & 0x7f) == 0) {
      g_hw.hdma[c].lines = rd8(bank | t++);
      if (indirect) {
        uint16_t ind = rd8(bank | t) | rd8(bank | (uint16_t)(t + 1)) << 8;
        t += 2;
        g_hw.hdma[c].indirect = (uint32_t)r[7] << 16 | ind;
      }
      if (g_hw.hdma[c].lines == 0) g_hw.hdma[c].active = 0;
      g_hw.hdma[c].do_transfer = 1;
    }
    g_hw.hdma[c].table = bank | t;
  }
}

// ---- CPU registers ----

static void cpu_reg_write(uint16_t adr, uint8_t val) {
  switch (adr) {
    case 0x4200: g_hw.nmitimen = val; break;
    case 0x4202: g_hw.multiply_a = val; break;
    case 0x4203: g_hw.multiply_result = (uint16_t)(g_hw.multiply_a * val); break;
    case 0x4204: g_hw.divide_a = (g_hw.divide_a & 0xff00) | val; break;
    case 0x4205: g_hw.divide_a = (uint16_t)((g_hw.divide_a & 0x00ff) | val << 8); break;
    case 0x4206:
      if (val == 0) {
        g_hw.divide_result = 0xffff;
        g_hw.multiply_result = g_hw.divide_a;
      } else {
        // 16-bit by 8-bit, by shifts: the ARM7 has no divide instruction.
        uint32_t q = 0, r = 0;
        for (int i = 15; i >= 0; i--) {
          r = (r << 1) | ((g_hw.divide_a >> i) & 1);
          if (r >= val) { r -= val; q |= 1u << i; }
        }
        g_hw.divide_result = (uint16_t)q;
        g_hw.multiply_result = (uint16_t)r;
      }
      break;
    case 0x420b: do_dma(val); break;
    case 0x420c: g_hw.hdmaen = val; break;
    default:
      if (adr >= 0x4300 && adr < 0x4380) g_hw.dma[(adr >> 4) & 7][adr & 0xf] = val;
      break;
  }
}

static uint8_t cpu_reg_read(uint16_t adr) {
  switch (adr) {
    case 0x4210: {
      uint8_t v = (uint8_t)(0x02 | g_hw.in_nmi << 7);
      g_hw.in_nmi = 0;
      return v;
    }
    case 0x4211: return 0;
    case 0x4212: return 0x80;  // in VBlank, auto-joypad done (see snes_hw.h)
    case 0x4214: return (uint8_t)g_hw.divide_result;
    case 0x4215: return g_hw.divide_result >> 8;
    case 0x4216: return (uint8_t)g_hw.multiply_result;
    case 0x4217: return g_hw.multiply_result >> 8;
    case 0x4218: case 0x421a: case 0x421c: case 0x421e: return (uint8_t)g_hw.joy[(adr - 0x4218) >> 1];
    case 0x4219: case 0x421b: case 0x421d: case 0x421f: return g_hw.joy[(adr - 0x4219) >> 1] >> 8;
    default:
      if (adr >= 0x4300 && adr < 0x4380) return g_hw.dma[(adr >> 4) & 7][adr & 0xf];
      return g_hw.open_bus;
  }
}

// ---- the memory map (HiROM) ----
// The common cases are inline in snes_mem.h; these do all of them.

HOT_CODE uint8_t rd8_slow(uint32_t a) {
  uint8_t bank = a >> 16;
  uint16_t lo = (uint16_t)a;
  uint8_t v;
  if ((bank & 0xfe) == 0x7e) return g_wram[a & 0x1ffff];
  if ((bank & 0x7f) < 0x40) {
    if (lo < 0x2000) return g_wram[lo];
    if (lo >= 0x8000) return g_rom[(((uint32_t)(bank & 0x3f) << 16) | lo) & g_rom_mask];
    if (lo >= 0x6000) v = g_sram[(((bank & 0x3f) << 13) | (lo & 0x1fff)) & 0x7ff];
    else if (lo == 0x4016) v = serial_read(0) | (g_hw.open_bus & 0xfc);
    else if (lo == 0x4017) v = serial_read(1) | (g_hw.open_bus & 0xe0) | 0x1c;
    else if (lo >= 0x2100 && lo < 0x2200) v = bbus_read((uint8_t)lo);
    else if (lo >= 0x4200 && lo < 0x4400) v = cpu_reg_read(lo);
    else v = g_hw.open_bus;
    g_hw.open_bus = v;
    return v;
  }
  return g_rom[(((uint32_t)(bank & 0x3f) << 16) | lo) & g_rom_mask];
}

HOT_CODE void wr8_slow(uint32_t a, uint8_t v) {
  uint8_t bank = a >> 16;
  uint16_t lo = (uint16_t)a;
  if ((bank & 0xfe) == 0x7e) { g_wram[a & 0x1ffff] = v; return; }
  if ((bank & 0x7f) < 0x40) {
    if (lo < 0x2000) g_wram[lo] = v;
    else if (lo >= 0x6000 && lo < 0x8000) g_sram[(((bank & 0x3f) << 13) | (lo & 0x1fff)) & 0x7ff] = v;
    else if (lo >= 0x2100 && lo < 0x2200) bbus_write((uint8_t)lo, v);
    else if (lo >= 0x4200 && lo < 0x4400) cpu_reg_write(lo, v);
    else if (lo == 0x4016) {
      g_hw.strobe = v & 1;
      if (g_hw.strobe) {
        g_hw.serial[0] = serial_order(g_hw.joy1_now);
        g_hw.serial[1] = 0;
      }
    }
  }
}
