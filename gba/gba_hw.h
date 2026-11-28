// GBA registers the port uses.
#ifndef GBA_HW_H
#define GBA_HW_H

#include <stdint.h>

#define REG16(a) (*(volatile uint16_t *)(a))
#define REG_DISPCNT REG16(0x04000000)
#define REG_VCOUNT REG16(0x04000006)
#define REG_KEYINPUT REG16(0x04000130)
#define REG_TM2CNT_L REG16(0x04000108)
#define REG_TM2CNT_H REG16(0x0400010a)
#define REG_TM3CNT_L REG16(0x0400010c)
#define REG_TM3CNT_H REG16(0x0400010e)
#define REG_WAITCNT REG16(0x04000204)
#define REG_DISPSTAT REG16(0x04000004)
#define REG_IE REG16(0x04000200)
#define REG_IF REG16(0x04000202)
#define REG_IME REG16(0x04000208)
#define REG_BIOS_IF REG16(0x03007ff8)

enum {
  KEY_A = 1 << 0, KEY_B = 1 << 1, KEY_SELECT = 1 << 2, KEY_START = 1 << 3,
  KEY_RIGHT = 1 << 4, KEY_LEFT = 1 << 5, KEY_UP = 1 << 6, KEY_DOWN = 1 << 7,
  KEY_R = 1 << 8, KEY_L = 1 << 9,
};

// EWRAM for big arrays the program clears itself.
#ifndef EWRAM_DATA
#define EWRAM_DATA __attribute__((section(".ewram_bss")))
#endif

// Hot code and data go in IWRAM.
#define IWRAM_DATA __attribute__((section(".iwram")))

// mGBA's debug output (gba/debug.c).
void debug_init(void);
void debug_print(const char *s);
void debug_print_sample(uint32_t v);   // 'S' and v in hex, from interrupts
// printf-like, with %d %u %x %08x %06x %s only.
void debug_printf(const char *fmt, ...);

// A 32-bit count of CPU cycles (timers 2 and 3, cascaded).
void cycles_start(void);
uint32_t cycles_now(void);

#endif
