// snesrun: runs the original ROM headless on LakeSnes, the port's reference.
//
//   snesrun ROM [options as environment variables]
//
// FRAMES=N        frames to run (default 600)
// KEYS=SCRIPT     joypad 1 script (tools/common/tool_common.h)
// OUT=DIR         where pictures and dumps go (default .)
// SHOTS=F1,F2     save the picture at these frames (shot_FFFFF.png, 256x224)
// DUMPS=F1,F2     save WRAM (wram_FFFFF.bin), VRAM, CGRAM, OAM at these frames
// TRACE=FILE      write what the CPU did, for the recompiler (see below)
// SYNC=FILE       write a line per frame: frame, hash of WRAM when the game
//                 first waits (WAI) in that frame, and the WAI's address
//
// A frame ends when the SNES enters VBlank. Pictures and dumps are taken
// then, after the frame's NMI has been requested but before it runs.
//
// The trace (TRACE) has a line per executed instruction address:
//   PC FLAGS DB DP
// FLAGS: bit0 M=1 seen, bit1 M=0 seen, bit2 X=1 seen, bit3 X=0 seen,
// bit4 E=1 seen. DB and DP are the values seen there first, with a '*'
// if others were seen too. And a line per dynamic control transfer seen
// (RTS, RTL, RTI, indirect JMP/JML/JSR) and per interrupt entry:
//   E FROM TO
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/lakesnes/snes.h"
#include "../third_party/lakesnes/cpu.h"
#include "common/tool_common.h"

// ---- tracing ----

typedef struct {
  uint8_t flags[0x10000];
  uint8_t db[0x10000];
  uint16_t dp[0x10000];
  uint8_t multi[0x10000]; // bit0 db differs, bit1 dp differs
} BankTrace;

static BankTrace *g_bank_trace[256];
static uint32_t g_prev_pc = 0xffffffff;
static uint8_t g_prev_op;

typedef struct { uint32_t from, to; } Edge;
static Edge *g_edges;
static size_t g_num_edges, g_cap_edges;

static void add_edge(uint32_t from, uint32_t to) {
  // Edges repeat heavily; a small hash set keeps it cheap.
  static uint64_t *set;
  static size_t set_cap = 1 << 20;
  if (!set) set = calloc(set_cap, sizeof(uint64_t));
  uint64_t key = ((uint64_t)from << 24 | to) + 1;
  size_t h = (key * 0x9E3779B97F4A7C15ull) >> 44;
  while (set[h & (set_cap - 1)]) {
    if (set[h & (set_cap - 1)] == key) return;
    h++;
  }
  set[h & (set_cap - 1)] = key;
  if (g_num_edges == g_cap_edges) {
    g_cap_edges = g_cap_edges ? g_cap_edges * 2 : 4096;
    g_edges = realloc(g_edges, g_cap_edges * sizeof(Edge));
  }
  g_edges[g_num_edges++] = (Edge){from, to};
}

static bool is_dynamic_transfer(uint8_t op) {
  return op == 0x60 || op == 0x6B || op == 0x40 || op == 0x6C || op == 0x7C ||
         op == 0xDC || op == 0xFC;
}

static Snes *g_snes;
static int g_frame;
static FILE *g_sync;
static bool g_sync_done_this_frame;

static uint32_t hash_ram(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
  return h;
}

static void trace_hook(Cpu *cpu) {
  uint32_t pc = (uint32_t)cpu->k << 16 | cpu->pc;
  BankTrace *bt = g_bank_trace[cpu->k];
  if (!bt) bt = g_bank_trace[cpu->k] = calloc(1, sizeof(BankTrace));
  uint16_t lo = cpu->pc;
  uint8_t f = (cpu->mf ? 1 : 2) | (cpu->xf ? 4 : 8) | (cpu->e ? 16 : 0);
  if (!bt->flags[lo]) {
    bt->db[lo] = cpu->db;
    bt->dp[lo] = cpu->dp;
  } else {
    if (bt->db[lo] != cpu->db) bt->multi[lo] |= 1;
    if (bt->dp[lo] != cpu->dp) bt->multi[lo] |= 2;
  }
  bt->flags[lo] |= f;
  if (g_prev_pc != 0xffffffff && is_dynamic_transfer(g_prev_op)) add_edge(g_prev_pc, pc);
  g_prev_pc = pc;
  g_prev_op = cpu->read(cpu->mem, pc);
}

// Called for every opcode when SYNC is on: notes the first WAI of a frame.
static void sync_hook(Cpu *cpu) {
  if (g_bank_trace[0] || cpu_traceHook == trace_hook) {}
  uint32_t pc = (uint32_t)cpu->k << 16 | cpu->pc;
  uint8_t op = cpu->read(cpu->mem, pc);
  if (op == 0xCB && !g_sync_done_this_frame) {
    fprintf(g_sync, "%d %08x %06x\n", g_frame, hash_ram(g_snes->ram, sizeof(g_snes->ram)), pc);
    g_sync_done_this_frame = true;
  }
}

static void both_hooks(Cpu *cpu) {
  trace_hook(cpu);
  sync_hook(cpu);
}

static void write_trace(const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
  for (int b = 0; b < 256; b++) {
    BankTrace *bt = g_bank_trace[b];
    if (!bt) continue;
    for (int i = 0; i < 0x10000; i++) {
      if (!bt->flags[i]) continue;
      fprintf(f, "%06x %02x %02x%s %04x%s\n", b << 16 | i, bt->flags[i], bt->db[i],
              bt->multi[i] & 1 ? "*" : "", bt->dp[i], bt->multi[i] & 2 ? "*" : "");
    }
  }
  for (size_t i = 0; i < g_num_edges; i++) fprintf(f, "E %06x %06x\n", g_edges[i].from, g_edges[i].to);
  fclose(f);
}

// ---- input ----

static void set_joypad(Snes *snes, uint16_t joy) {
  // LakeSnes keeps the buttons in shift order: bit 0 is B, the first read.
  uint16_t serial = 0;
  for (int i = 0; i < 16; i++)
    if (joy & (0x8000 >> i)) serial |= 1 << i;
  snes->input1->currentState = serial;
}

// ---- output ----

static void save_shot(Snes *snes, const char *dir, int frame) {
  static uint8_t pixels[512 * 480 * 4];
  static uint8_t rgb[256 * 224 * 3];
  snes_setPixels(snes, pixels);
  for (int y = 0; y < 224; y++)
    for (int x = 0; x < 256; x++) {
      // putPixels doubles the picture (512x480, 16 lines of border at the top).
      const uint8_t *p = pixels + ((y * 2 + 16) * 512 + x * 2) * 4;
      uint8_t *o = rgb + (y * 256 + x) * 3;
      o[0] = p[3]; o[1] = p[2]; o[2] = p[1]; // LakeSnes default: X, B, G, R
    }
  char path[1024];
  snprintf(path, sizeof(path), "%s/shot_%05d.png", dir, frame);
  write_png_rgb(path, rgb, 256, 224);
}

static void save_dump(Snes *snes, const char *dir, int frame) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/wram_%05d.bin", dir, frame);
  write_file(path, snes->ram, sizeof(snes->ram));
  snprintf(path, sizeof(path), "%s/vram_%05d.bin", dir, frame);
  write_file(path, snes->ppu->vram, sizeof(snes->ppu->vram));
  snprintf(path, sizeof(path), "%s/cgram_%05d.bin", dir, frame);
  write_file(path, snes->ppu->cgram, sizeof(snes->ppu->cgram));
  snprintf(path, sizeof(path), "%s/oam_%05d.bin", dir, frame);
  write_file(path, snes->ppu->oam, sizeof(snes->ppu->oam));
  snprintf(path, sizeof(path), "%s/oamhigh_%05d.bin", dir, frame);
  write_file(path, snes->ppu->highOam, sizeof(snes->ppu->highOam));
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s ROM (see the source for the environment variables)\n", argv[0]);
    return 1;
  }
  size_t rom_size;
  uint8_t *rom = read_file(argv[1], &rom_size);
  int frames = getenv("FRAMES") ? atoi(getenv("FRAMES")) : 600;
  const char *out = getenv("OUT") ? getenv("OUT") : ".";
  static int shots[4096], dumps[4096];
  int num_shots = parse_int_list(getenv("SHOTS"), shots, 4096);
  int num_dumps = parse_int_list(getenv("DUMPS"), dumps, 4096);
  KeyScript keys;
  if (!keyscript_parse(&keys, getenv("KEYS"))) { fprintf(stderr, "bad KEYS\n"); return 1; }
  const char *trace_path = getenv("TRACE");
  if (getenv("SYNC")) g_sync = fopen(getenv("SYNC"), "w");

  Snes *snes = g_snes = snes_init();
  if (!snes_loadRom(snes, rom, (int)rom_size)) { fprintf(stderr, "cannot load the ROM\n"); return 1; }
  if (trace_path && g_sync) cpu_traceHook = both_hooks;
  else if (trace_path) cpu_traceHook = trace_hook;
  else if (g_sync) cpu_traceHook = sync_hook;

  for (g_frame = 0; g_frame < frames; g_frame++) {
    set_joypad(snes, keyscript_at(&keys, g_frame));
    g_sync_done_this_frame = false;
    snes_runFrame(snes);
    if (int_list_has(shots, num_shots, g_frame)) save_shot(snes, out, g_frame);
    if (int_list_has(dumps, num_dumps, g_frame)) save_dump(snes, out, g_frame);
  }
  if (trace_path) write_trace(trace_path);
  if (g_sync) fclose(g_sync);
  return 0;
}
