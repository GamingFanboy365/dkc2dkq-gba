// hostrun: the recompiled game on the PC, on LakeSnes's hardware.
//
//   hostrun ROM   (options as environment variables, as tools/snesrun.c)
//
// FRAMES, KEYS, OUT, SHOTS, DUMPS: as snesrun, so pictures and dumps of the
// same frames can be compared with the reference's.
// SYNC=FILE  a line per NMI, at the first WAI after it: frame, hash of WRAM,
//            the WAI's address (snesrun's format; compare the two with
//            tools/compare_sync.py)
// WAIT_DUMPS=N,.. with SYNC, save WRAM at the Nth line's WAI (waitwram_N.bin)
//
// The game's code is the recompiled C (gen/, from tools/recomp); LakeSnes
// provides everything else: PPU, DMA and HDMA, the APU, joypads, the
// multiplier. Its own CPU isn't used.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/lakesnes/snes.h"
#include "../third_party/lakesnes/dma.h"
#include "../tools/common/tool_common.h"
#include "cpu65.h"

uint64_t g_cpu_cycles, g_event_at;
static Snes *hw;
static int g_frame, g_frames;
static bool g_was_vblank;
static KeyScript g_keys;
static int g_shots[4096], g_num_shots, g_dumps[4096], g_num_dumps;
static const char *g_out = ".";
static FILE *g_sync;
static bool g_stop;
static int g_num_waits, g_wait_dumps[4096], g_num_wait_dumps;
static bool g_nmi_since_wait = true;

static uint32_t hash_ram(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ (i == 0x2c || i == 0x2d ? 0 : p[i])) * 16777619u;
  return h;
}

static void set_joypad(uint16_t joy) {
  uint16_t serial = 0;
  for (int i = 0; i < 16; i++)
    if (joy & (0x8000 >> i)) serial |= 1 << i;
  hw->input1->currentState = serial;
}

static void save_shot(int frame) {
  static uint8_t pixels[512 * 480 * 4];
  static uint8_t rgb[256 * 224 * 3];
  snes_setPixels(hw, pixels);
  for (int y = 0; y < 224; y++)
    for (int x = 0; x < 256; x++) {
      const uint8_t *p = pixels + ((y * 2 + 16) * 512 + x * 2) * 4;
      uint8_t *o = rgb + (y * 256 + x) * 3;
      o[0] = p[3]; o[1] = p[2]; o[2] = p[1];  // LakeSnes default: X, B, G, R
    }
  char path[1024];
  snprintf(path, sizeof(path), "%s/shot_%05d.png", g_out, frame);
  write_png_rgb(path, rgb, 256, 224);
}

static void save_dump(int frame) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/wram_%05d.bin", g_out, frame);
  write_file(path, hw->ram, sizeof(hw->ram));
}

// A frame ends when VBlank starts, as in snesrun.
static void on_frame_end(void) {
  if (int_list_has(g_shots, g_num_shots, g_frame)) save_shot(g_frame);
  if (int_list_has(g_dumps, g_num_dumps, g_frame)) save_dump(g_frame);
  g_frame++;
  if (g_frame >= g_frames) g_stop = true;
  set_joypad(keyscript_at(&g_keys, g_frame));
}

// Runs the hardware up to the CPU's clock.
static void catch_up(void) {
  while (hw->cycles < g_cpu_cycles) {
    dma_handleDma(hw->dma, 6);
    snes_runCycles(hw, 6);
    if (hw->inVblank && !g_was_vblank) on_frame_end();
    g_was_vblank = hw->inVblank;
  }
  if (hw->cycles > g_cpu_cycles) g_cpu_cycles = hw->cycles;  // DMA took time
}

bool host_poll(void) {
  catch_up();
  g_event_at = g_cpu_cycles + 4 * 1364;
  return hw->cpu->nmiWanted || g_stop;
}

static bool is_io(uint32_t a) {
  uint8_t bank = a >> 16;
  uint16_t lo = a;
  return (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) && lo >= 0x2100 && lo < 0x6000;
}

uint8_t rd8(uint32_t a) {
  if (is_io(a)) catch_up();
  return snes_read(hw, a);
}

void wr8(uint32_t a, uint8_t v) {
  if (is_io(a)) {
    catch_up();
    snes_write(hw, a, v);
    if ((a & 0xffff) == 0x420b && v) {
      // LakeSnes starts a DMA on the CPU's next access; do it now.
      dma_handleDma(hw->dma, 6);
      dma_handleDma(hw->dma, 6);
      catch_up();
    }
    return;
  }
  snes_write(hw, a, v);
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
  if (argc < 2) {
    fprintf(stderr, "usage: %s ROM\n", argv[0]);
    return 1;
  }
  size_t rom_size;
  uint8_t *rom = read_file(argv[1], &rom_size);
  g_frames = getenv("FRAMES") ? atoi(getenv("FRAMES")) : 600;
  if (getenv("OUT")) g_out = getenv("OUT");
  g_num_shots = parse_int_list(getenv("SHOTS"), g_shots, 4096);
  g_num_dumps = parse_int_list(getenv("DUMPS"), g_dumps, 4096);
  if (!keyscript_parse(&g_keys, getenv("KEYS"))) { fprintf(stderr, "bad KEYS\n"); return 1; }
  if (getenv("SYNC")) g_sync = fopen(getenv("SYNC"), "w");
  g_num_wait_dumps = parse_int_list(getenv("WAIT_DUMPS"), g_wait_dumps, 4096);

  hw = snes_init();
  if (!snes_loadRom(hw, rom, (int)rom_size)) { fprintf(stderr, "cannot load the ROM\n"); return 1; }
  set_joypad(keyscript_at(&g_keys, 0));

  // The 65816 after reset.
  memset(&cpu, 0, sizeof(cpu));
  cpu.e = 1; cpu.mf = 1; cpu.xf = 1; cpu.i = 1; cpu.s = 0x1ff;
  uint32_t pc = rd16(0xfffc);
  g_event_at = 4 * 1364;

  long yields = 0, interrupts = 0;
  while (!g_stop) {
    uint32_t r = run_until_flag(pc);
    if (g_stop) break;
    if (r & PC_STOP) {
      fprintf(stderr, "frame %d: stopped at %06X\n", g_frame, r & 0xffffff);
      break;
    }
    uint32_t resume = r & 0xffffff;
    if (r & PC_YIELD) {
      yields++;
      if (g_sync && g_nmi_since_wait) {
        g_nmi_since_wait = false;
        // The WAI is the instruction before the resume address.
        fprintf(g_sync, "%d %08x %06x\n", g_frame, hash_ram(hw->ram, sizeof(hw->ram)),
                (resume & 0xff0000) | ((resume - 1) & 0xffff));
        if (int_list_has(g_wait_dumps, g_num_wait_dumps, g_num_waits)) {
          char path[1024];
          snprintf(path, sizeof(path), "%s/waitwram_%d.bin", g_out, g_num_waits);
          write_file(path, hw->ram, sizeof(hw->ram));
        }
        g_num_waits++;
      }
      // Wait for the interrupt.
      while (!hw->cpu->nmiWanted && !g_stop) {
        g_cpu_cycles += 6 * 7;
        catch_up();
      }
      if (g_stop) break;
    }
    if (hw->cpu->nmiWanted) {
      hw->cpu->nmiWanted = false;
      interrupts++;
      g_nmi_since_wait = true;
      pc = interrupt(0xffea, resume);
    } else {
      pc = resume;  // polled for something else (the frame limit)
    }
  }
  if (g_sync) fclose(g_sync);
  fprintf(stderr, "%d frames, %ld waits, %ld NMIs\n", g_frame, yields, interrupts);
  return 0;
}
