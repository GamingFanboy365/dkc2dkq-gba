// A model of the SNES around the CPU, for the recompiled game: the memory
// map, the PPU's memories and ports, DMA, the multiplier and divider, the
// joypads and the APU's ports. It's what the GBA build runs on, and the PC's
// `hostmini` checks it against the reference.
//
// What it leaves out, for now: HDMA and the PPU's other registers are only
// recorded (regs[]) for a renderer to use; the APU runs no code: its port 0
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

typedef struct {
  uint16_t vram[0x8000];
  uint16_t cgram[256];
  uint16_t oam[256];
  uint8_t high_oam[32];
  uint8_t regs[0x40];        // the last value written to each $21xx PPU register
  // PPU port state
  uint16_t vram_pointer, vram_read_buffer, vram_increment;
  uint8_t vram_remap, vram_increment_on_high;
  uint8_t oam_adr, oam_adr_written, oam_in_high, oam_in_high_written, oam_second_write, oam_buffer;
  uint8_t cgram_pointer, cgram_second_write, cgram_buffer;
  int16_t m7matrix[2];
  uint8_t m7prev;
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
// The start of VBlank: the NMI flag, and the joypads read (auto-joypad).
void hw_vblank(uint16_t joy1);
static inline bool hw_nmi_enabled(void) { return g_hw.nmitimen & 0x80; }

#endif
