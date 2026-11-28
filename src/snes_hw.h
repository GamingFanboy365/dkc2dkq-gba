// A model of the SNES around the CPU, for the recompiled game: the memory
// map, the PPU's memories and ports, DMA, the multiplier and divider, the
// joypads and the APU's ports. It's what the GBA build runs on, and the PC's
// `hostmini` checks it against the reference.
//
// The PPU draws nothing here: its registers are recorded (regs[], and
// decoded where a register takes two writes) for a renderer. HDMA is run a
// frame at a time by the platform: hw_hdma_begin, then hw_hdma_line before
// each visible line, which writes that line's registers as the SNES would.
//
// What it leaves out, for now: the APU runs no code: its port 0
// answers as the IPL ROM does while the game uploads the sound driver (the
// $CC start, an echo of the counter per byte, the jump), reads 0 once the
// driver starts, and then echoes what the CPU writes, which is all the game's
// handshakes wait for (it reads no other port); no sound yet. Timing: VBlank
// is always on as far as $4212 says.
// Port semantics follow LakeSnes (third_party/lakesnes), the reference.
#ifndef SNES_HW_H
#define SNES_HW_H

#include <stdint.h>
#include <stdbool.h>

// Hot code: on the GBA, ARM in IWRAM (gba/platform_cpu.h sets HOT_CODE).
#ifndef HOT_CODE
#define HOT_CODE
#endif
#ifndef WARM_CODE
#define WARM_CODE
#endif

typedef struct {
  uint16_t vram[0x8000];
  // A bit per 16 bytes of VRAM, set by every write there, for a renderer
  // that converts tiles to see which changed (it clears them).
  uint32_t vram_dirty[0x1000 / 32];
  uint16_t cgram[256];
  uint16_t cgram_ver;        // bumped on every CGRAM write, for renderers
  uint16_t oam[256];
  uint8_t high_oam[32];
  uint8_t regs[0x40];        // the last value written to each $21xx PPU register
  // PPU port state
  uint16_t vram_pointer, vram_read_buffer, vram_increment;
  uint8_t vram_remap, vram_increment_on_high;
  uint8_t oam_adr, oam_adr_written, oam_in_high, oam_in_high_written, oam_second_write, oam_buffer;
  uint8_t cgram_pointer, cgram_second_write, cgram_buffer;
  int16_t m7matrix[8];       // A, B, C, D, X, Y, HOFS, VOFS ($211B-$2120, $210D/E)
  uint8_t m7prev;
  // Decoded registers (the write-twice ones, and COLDATA's three channels)
  uint16_t hofs[4], vofs[4];
  uint8_t scroll_prev, scroll_prev2;
  uint8_t fixed_r, fixed_g, fixed_b;
  // HDMA, while a frame's lines are run (hw_hdma_*)
  struct {
    uint32_t table;           // the next table byte to read
    uint32_t indirect;        // indirect mode: the next data byte
    uint8_t lines;            // the line counter (bit 7: repeat)
    uint8_t active, do_transfer;
  } hdma[8];
  // CPU registers
  uint8_t nmitimen, hdmaen, in_nmi;
  uint8_t multiply_a;
  uint16_t multiply_result, divide_a, divide_result;
  uint16_t joy[4];
  uint16_t joy1_now;         // the buttons held, for $4016's serial reads
  uint16_t serial[2];        // $4016/$4017's shift registers, B first
  uint8_t strobe;
  uint32_t wram_adr;
  uint8_t dma[8][16];        // $43x0-$43xF
  // APU ports: what the CPU wrote, what it reads, and the IPL ROM's state
  uint8_t apu_in[4], apu_out[4];
  uint8_t apu_state, apu_counter, apu_pending;
  uint8_t open_bus;
} SnesHw;

extern SnesHw g_hw;
extern uint8_t g_wram[0x20000];
extern uint8_t g_sram[0x800];
extern const uint8_t *g_rom;
extern uint32_t g_rom_mask;   // ROM size - 1 (a power of two)

void hw_reset(void);
// HDMA for one frame: begin at the top, then a call per visible line (0 to
// 223) in order, before the line is drawn. Every PPU register write, the
// game's and HDMA's, goes to g_ppu_write_hook too when it's set.
void hw_hdma_begin(void);
void hw_hdma_line(void);
// The same for a whole frame: begin, then the 224 lines, calling
// after_line(line) after each line's transfers (NULL: none).
void hw_hdma_frame(void (*after_line)(int line));
// In hw_hdma_frame's after_line: what HDMA wrote on that line (0: nothing).
#define HDMA_WROTE_SCROLL 1   // BG1-4 HOFS/VOFS
#define HDMA_WROTE_FIXED 2    // COLDATA
#define HDMA_WROTE_LAYERS 4   // TM, TS
#define HDMA_WROTE_CGRAM 8    // CGADD, CGDATA
#define HDMA_WROTE_OTHER 0x80
extern uint8_t g_hdma_wrote;
// hw_hdma_frame for the simplest frames (only scroll and COLDATA writes),
// channel by channel: the lines' BG1-3 scroll and fixed color (r | g << 5 |
// b << 10) into the arrays. false for other frames, having done nothing.
bool hw_hdma_frame_fast(uint16_t hofs[3][224], uint16_t vofs[3][224], uint16_t *fixed);
extern void (*g_ppu_write_hook)(uint8_t reg, uint8_t value);
// The start of VBlank: the NMI flag, and the joypads read (auto-joypad).
void hw_vblank(uint16_t joy1);
static inline bool hw_nmi_enabled(void) { return g_hw.nmitimen & 0x80; }

#endif
