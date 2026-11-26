// Shared helpers for the host tools: files, PNG output, the button script.
#ifndef TOOL_COMMON_H
#define TOOL_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Reads a whole file; exits on failure.
uint8_t *read_file(const char *path, size_t *size);
bool write_file(const char *path, const void *data, size_t size);

// Writes 8-bit RGB pixels as a PNG.
bool write_png_rgb(const char *path, const uint8_t *rgb, int w, int h);

// SNES joypad bits, as the game reads them from $4218 (JOY1L/H).
enum {
  SNES_B = 0x8000, SNES_Y = 0x4000, SNES_SELECT = 0x2000, SNES_START = 0x1000,
  SNES_UP = 0x0800, SNES_DOWN = 0x0400, SNES_LEFT = 0x0200, SNES_RIGHT = 0x0100,
  SNES_A = 0x0080, SNES_X = 0x0040, SNES_L = 0x0020, SNES_R = 0x0010,
};

// A button script: "N:KEYS,N:KEYS,...", each holding KEYS (names joined by
// '+', or '-' for none) for N frames. Names: A B X Y L R Up Down Left Right
// Select Start. A trailing "*" in place of N repeats the last entry forever.
typedef struct {
  int count;
  int frames[1024];
  uint16_t keys[1024];
} KeyScript;

bool keyscript_parse(KeyScript *ks, const char *text);
// The joypad for a given frame (0 after the script ends).
uint16_t keyscript_at(const KeyScript *ks, int frame);

// A comma-separated list of integers (frame numbers), sorted on return.
int parse_int_list(const char *text, int *out, int max);
bool int_list_has(const int *list, int n, int value);

#endif
