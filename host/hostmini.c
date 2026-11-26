// hostmini: the recompiled game on the PC, on the port's own hardware model
// (src/snes_hw.c), the one the GBA build uses. Checks the model against the
// reference: the same SYNC log as snesrun and hostrun.
//
//   hostmini ROM   (FRAMES, KEYS, SYNC as snesrun; DUMPS writes WRAM, VRAM,
//                   CGRAM at the given frames' waits, OUT=DIR; REGS=N,..
//                   prints the PPU's registers and HDMA setup at the game's
//                   Nth waits; SHOTS=N,.. saves the picture a SNES would show
//                   from the model's state at the game's Nth waits
//                   (mini_shot_N.png): LakeSnes's PPU, given the model's
//                   VRAM, CGRAM, OAM and registers, with the model's HDMA
//                   run line by line; HIDE=MASK leaves layers out of them:
//                   bit 0 BG1 ... bit 3 BG4, bit 4 sprites)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/common/tool_common.h"
#include "cpu65.h"
#include "game_loop.h"
#include "snes_hw.h"
#include "../third_party/lakesnes/snes.h"
#include "sync_hash.h"

static int g_frame, g_num_waits;
static KeyScript g_keys;
static FILE *g_sync;
static int g_dumps[4096], g_num_dumps;
static int g_regdumps[4096], g_num_regdumps;   // REGS=N,..: print the PPU's registers at these waits
static const char *g_out = ".";


// ---- the SNES's picture of the model's state ----

static Snes *g_ref;
static int g_shots[4096], g_num_shots;

static int g_hide_layers;   // HIDE=mask: leave these layers out of the pictures (bit 0 BG1 .. 4 OBJ)

static void ref_write(uint8_t reg, uint8_t value) {
  if (reg == 0x2c || reg == 0x2d) value &= ~g_hide_layers;
  ppu_write(g_ref->ppu, reg, value);
}

static void ref_load_state(void) {
  Ppu *ppu = g_ref->ppu;
  ppu_reset(ppu);
  memcpy(ppu->vram, g_hw.vram, sizeof(ppu->vram));
  memcpy(ppu->cgram, g_hw.cgram, sizeof(ppu->cgram));
  memcpy(ppu->oam, g_hw.oam, sizeof(ppu->oam));
  memcpy(ppu->highOam, g_hw.high_oam, sizeof(ppu->highOam));
  for (int r = 0; r < 0x34; r++) {
    if ((r >= 0x02 && r <= 0x04) || (r >= 0x0d && r <= 0x22) || r == 0x32) continue;
    ref_write((uint8_t)r, g_hw.regs[r]);
  }
  for (int i = 0; i < 4; i++) {
    ppu_write(ppu, (uint8_t)(0x0d + i * 2), (uint8_t)g_hw.hofs[i]);
    ppu_write(ppu, (uint8_t)(0x0d + i * 2), g_hw.hofs[i] >> 8);
    ppu_write(ppu, (uint8_t)(0x0e + i * 2), (uint8_t)g_hw.vofs[i]);
    ppu_write(ppu, (uint8_t)(0x0e + i * 2), g_hw.vofs[i] >> 8);
  }
  ppu_write(ppu, 0x1a, g_hw.regs[0x1a]);
  for (int i = 0; i < 6; i++) {
    ppu_write(ppu, (uint8_t)(0x1b + i), (uint8_t)g_hw.m7matrix[i]);
    ppu_write(ppu, (uint8_t)(0x1b + i), (uint8_t)(g_hw.m7matrix[i] >> 8));
  }
  // Mode 7's scroll shares $210D/$210E with BG1's; set it last.
  ppu->m7matrix[6] = g_hw.m7matrix[6];
  ppu->m7matrix[7] = g_hw.m7matrix[7];
  ppu->m7prev = g_hw.m7prev;
  ppu_write(ppu, 0x32, 0x20 | g_hw.fixed_r);
  ppu_write(ppu, 0x32, 0x40 | g_hw.fixed_g);
  ppu_write(ppu, 0x32, 0x80 | g_hw.fixed_b);
  // The scroll latches as the model has them, for HDMA's scroll writes.
  ppu->scrollPrev = g_hw.scroll_prev;
  ppu->scrollPrev2 = g_hw.scroll_prev2;
}

static void draw_line(int line) { ppu_runLine(g_ref->ppu, line + 1); }

// Runs the frame's HDMA on the model, and draws the frame if asked.
static void run_display(bool draw) {
  if (draw) {
    if (!g_ref) g_ref = snes_init();
    ref_load_state();
    g_ppu_write_hook = ref_write;
    ppu_handleFrameStart(g_ref->ppu);
  }
  // The SNES draws a frame while the game's logic runs, and the logic
  // rewrites some HDMA tables in WRAM (the clouds' and the sea's scroll in
  // Pirate Panic) as it goes, so lines drawn before the rewrite use the old
  // values. This draws every line from the tables as the logic left them:
  // where the rewrite came partway down the SNES's screen, some lines differ
  // by the scroll's change of a frame (a pixel). Matching it would take the
  // logic's timing to the scanline.
  hw_hdma_frame(draw ? draw_line : NULL);
  g_ppu_write_hook = NULL;
  if (draw) {
    static uint8_t pixels[512 * 480 * 4];
    static uint8_t rgb[256 * 224 * 3];
    ppu_handleVblank(g_ref->ppu);
    ppu_putPixels(g_ref->ppu, pixels);
    for (int y = 0; y < 224; y++)
      for (int x = 0; x < 256; x++) {
        const uint8_t *px = pixels + ((y * 2 + 16) * 512 + x * 2) * 4;
        uint8_t *o = rgb + (y * 256 + x) * 3;
        o[0] = px[3]; o[1] = px[2]; o[2] = px[1];
      }
    char path[1024];
    snprintf(path, sizeof(path), "%s/mini_shot_%d.png", g_out, g_num_waits);
    write_png_rgb(path, rgb, 256, 224);
  }
}

uint16_t on_wait(uint32_t wai_pc) {
  if (g_sync) fprintf(g_sync, "%d %08x %06x\n", g_frame, sync_hash(g_wram), wai_pc);
  if (int_list_has(g_dumps, g_num_dumps, g_frame)) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/mini_wram_%05d.bin", g_out, g_frame);
    write_file(path, g_wram, sizeof(g_wram));
    snprintf(path, sizeof(path), "%s/mini_vram_%05d.bin", g_out, g_frame);
    write_file(path, g_hw.vram, sizeof(g_hw.vram));
    snprintf(path, sizeof(path), "%s/mini_cgram_%05d.bin", g_out, g_frame);
    write_file(path, g_hw.cgram, sizeof(g_hw.cgram));
  }
  if (int_list_has(g_regdumps, g_num_regdumps, g_num_waits)) {
    printf("wait %d (frame %d): PPU", g_num_waits, g_frame);
    for (int i = 0; i < 0x34; i++) printf("%s%02x", i % 16 == 0 ? "\n  " : " ", g_hw.regs[i]);
    printf("\n  NMITIMEN %02x HDMAEN %02x\n", g_hw.nmitimen, g_hw.hdmaen);
    for (int c = 0; c < 8; c++)
      if (g_hw.hdmaen & (1 << c)) {
        const uint8_t *d = g_hw.dma[c];
        uint32_t table = d[4] << 16 | d[3] << 8 | d[2];
        printf("  HDMA %d: mode %02x -> $21%02x, table %06x, indirect bank %02x:", c, d[0], d[1], table, d[7]);
        for (int i = 0; i < 12; i++) printf(" %02x", rd8(table + i));
        printf("\n");
      }
  }
  run_display(int_list_has(g_shots, g_num_shots, g_num_waits));
  return keyscript_at(&g_keys, g_num_waits++);
}

void unknown_entry(uint32_t pc) {
  fprintf(stderr, "frame %d: no recompiled code to enter at %06X\n", g_frame, pc);
}

void assume_failed(uint32_t pc, int m, int x) {
  static uint32_t seen[256];
  static int n;
  for (int i = 0; i < n; i++) if (seen[i] == pc) return;
  if (n < 256) seen[n++] = pc;
  fprintf(stderr, "frame %d: at %06X the recompiler assumed M=%d X=%d, but M=%d X=%d\n",
          g_frame, pc, m, x, cpu.mf, cpu.xf);
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: %s ROM\n", argv[0]); return 1; }
  size_t size;
  uint8_t *rom = read_file(argv[1], &size);
  g_rom = rom;
  g_rom_mask = (uint32_t)size - 1;
  int frames = getenv("FRAMES") ? atoi(getenv("FRAMES")) : 600;
  if (!keyscript_parse(&g_keys, getenv("KEYS"))) { fprintf(stderr, "bad KEYS\n"); return 1; }
  if (getenv("SYNC")) g_sync = fopen(getenv("SYNC"), "w");
  if (getenv("OUT")) g_out = getenv("OUT");
  g_num_dumps = parse_int_list(getenv("DUMPS"), g_dumps, 4096);
  g_num_regdumps = parse_int_list(getenv("REGS"), g_regdumps, 4096);
  g_num_shots = parse_int_list(getenv("SHOTS"), g_shots, 4096);
  if (getenv("HIDE")) g_hide_layers = (int)strtol(getenv("HIDE"), NULL, 0);
  game_reset();
  for (g_frame = 0; g_frame < frames; g_frame++) {
    if (!game_frame()) {
      fprintf(stderr, "frame %d: the game stopped\n", g_frame);
      break;
    }
  }
  if (g_sync) fclose(g_sync);
  fprintf(stderr, "%d frames\n", g_frame);
  return 0;
}
