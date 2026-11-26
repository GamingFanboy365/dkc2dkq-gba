// hostmini: the recompiled game on the PC, on the port's own hardware model
// (src/snes_hw.c), the one the GBA build uses. Checks the model against the
// reference: the same SYNC log as snesrun and hostrun.
//
//   hostmini ROM   (FRAMES, KEYS, SYNC as snesrun; DUMPS writes WRAM, VRAM,
//                   CGRAM at the given frames' waits, OUT=DIR)
#include <stdio.h>
#include <stdlib.h>

#include "../tools/common/tool_common.h"
#include "cpu65.h"
#include "game_loop.h"
#include "snes_hw.h"
#include "sync_hash.h"

static int g_frame, g_num_waits;
static KeyScript g_keys;
static FILE *g_sync;
static int g_dumps[4096], g_num_dumps;
static const char *g_out = ".";


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
