// mGBA's debug registers, and a printf small enough to have no division.
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "gba_hw.h"

#define REG_DEBUG_ENABLE REG16(0x04fff780)
#define REG_DEBUG_FLAGS REG16(0x04fff700)
#define REG_DEBUG_STRING ((volatile char *)0x04fff600)

static bool g_debug;

void debug_init(void) {
  REG_DEBUG_ENABLE = 0xc0de;
  g_debug = REG_DEBUG_ENABLE == 0x1dea;
}

void debug_print(const char *s) {
  if (!g_debug) return;
  int i = 0;
  for (; s[i] && i < 255; i++) REG_DEBUG_STRING[i] = s[i];
  REG_DEBUG_STRING[i] = 0;
  REG_DEBUG_FLAGS = 3 | 0x100;  // "info"
}

// "S" and a number in hex, written straight to the registers: no buffer,
// for interrupt handlers (the IRQ stack is 160 bytes).
void debug_print_sample(uint32_t v) {
  if (!g_debug) return;
  REG_DEBUG_STRING[0] = 'S';
  REG_DEBUG_STRING[1] = ' ';
  for (int i = 0; i < 8; i++) REG_DEBUG_STRING[2 + i] = "0123456789abcdef"[(v >> (28 - 4 * i)) & 15];
  REG_DEBUG_STRING[10] = 0;
  REG_DEBUG_FLAGS = 3 | 0x100;
}

static char *put_unsigned(char *p, uint32_t v) {
  static const uint32_t kPow10[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};
  bool started = false;
  for (int i = 0; i < 10; i++) {
    char d = '0';
    while (v >= kPow10[i]) { v -= kPow10[i]; d++; }
    if (d != '0' || started || i == 9) { *p++ = d; started = true; }
  }
  return p;
}

void debug_printf(const char *fmt, ...) {
  char buf[256];
  char *p = buf, *end = buf + sizeof(buf) - 12;
  va_list ap;
  va_start(ap, fmt);
  for (; *fmt && p < end; fmt++) {
    if (*fmt != '%') { *p++ = *fmt; continue; }
    fmt++;
    int width = 0;
    bool zero = false;
    if (*fmt == '0') { zero = true; fmt++; }
    while (*fmt >= '1' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
    switch (*fmt) {
      case 'd': {
        int v = va_arg(ap, int);
        if (v < 0) { *p++ = '-'; v = -v; }
        p = put_unsigned(p, (uint32_t)v);
        break;
      }
      case 'u': p = put_unsigned(p, va_arg(ap, uint32_t)); break;
      case 'x': {
        uint32_t v = va_arg(ap, uint32_t);
        int digits = 8;
        while (digits > 1 && !(v >> ((digits - 1) * 4))) digits--;
        if (zero && width > digits) digits = width;
        for (int i = digits - 1; i >= 0; i--) *p++ = "0123456789abcdef"[(v >> (i * 4)) & 15];
        break;
      }
      case 's': {
        const char *s = va_arg(ap, const char *);
        while (*s && p < end) *p++ = *s++;
        break;
      }
      default: *p++ = *fmt; break;
    }
  }
  va_end(ap);
  *p = 0;
  debug_print(buf);
}

void cycles_start(void) {
  REG_TM2CNT_H = 0;
  REG_TM3CNT_H = 0;
  REG_TM2CNT_L = 0;
  REG_TM3CNT_L = 0;
  REG_TM3CNT_H = 0x84;  // enabled, counts timer 2's overflows
  REG_TM2CNT_H = 0x80;  // enabled, every cycle
}

uint32_t cycles_now(void) {
  uint16_t hi, lo, hi2;
  do {
    hi = REG_TM3CNT_L;
    lo = REG_TM2CNT_L;
    hi2 = REG_TM3CNT_L;
  } while (hi != hi2);
  return (uint32_t)hi << 16 | lo;
}
