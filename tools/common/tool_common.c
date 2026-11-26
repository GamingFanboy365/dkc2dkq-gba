#include "tool_common.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

uint8_t *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *data = malloc(n ? n : 1);
  if (fread(data, 1, n, f) != (size_t)n) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
  fclose(f);
  if (size) *size = n;
  return data;
}

bool write_file(const char *path, const void *data, size_t size) {
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  bool ok = fwrite(data, 1, size, f) == size;
  fclose(f);
  return ok;
}

bool write_png_rgb(const char *path, const uint8_t *rgb, int w, int h) {
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct(png);
  if (setjmp(png_jmpbuf(png))) { fclose(f); return false; }
  png_init_io(png, f);
  png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (int y = 0; y < h; y++) png_write_row(png, (png_bytep)(rgb + y * w * 3));
  png_write_end(png, NULL);
  png_destroy_write_struct(&png, &info);
  fclose(f);
  return true;
}

static const struct { const char *name; uint16_t bit; } kKeyNames[] = {
  {"A", SNES_A}, {"B", SNES_B}, {"X", SNES_X}, {"Y", SNES_Y},
  {"L", SNES_L}, {"R", SNES_R}, {"Up", SNES_UP}, {"Down", SNES_DOWN},
  {"Left", SNES_LEFT}, {"Right", SNES_RIGHT}, {"Select", SNES_SELECT},
  {"Start", SNES_START},
};

bool keyscript_parse(KeyScript *ks, const char *text) {
  ks->count = 0;
  if (!text) return true;
  const char *p = text;
  while (*p) {
    if (ks->count >= 1024) return false;
    int n;
    if (*p == '*') { n = -1; p++; }
    else { n = (int)strtol(p, (char **)&p, 10); }
    if (*p++ != ':') return false;
    uint16_t keys = 0;
    while (*p && *p != ',') {
      const char *e = p;
      while (*e && *e != '+' && *e != ',') e++;
      size_t len = e - p;
      if (!(len == 1 && *p == '-')) {
        bool found = false;
        for (size_t i = 0; i < sizeof(kKeyNames) / sizeof(kKeyNames[0]); i++)
          if (strlen(kKeyNames[i].name) == len && !strncasecmp(p, kKeyNames[i].name, len)) {
            keys |= kKeyNames[i].bit; found = true;
          }
        if (!found) { fprintf(stderr, "unknown key in script: %.*s\n", (int)len, p); return false; }
      }
      p = e;
      if (*p == '+') p++;
    }
    ks->frames[ks->count] = n;
    ks->keys[ks->count] = keys;
    ks->count++;
    if (*p == ',') p++;
  }
  return true;
}

uint16_t keyscript_at(const KeyScript *ks, int frame) {
  int f = 0;
  for (int i = 0; i < ks->count; i++) {
    if (ks->frames[i] < 0) return ks->keys[i];
    if (frame < f + ks->frames[i]) return ks->keys[i];
    f += ks->frames[i];
  }
  return 0;
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int parse_int_list(const char *text, int *out, int max) {
  int n = 0;
  if (!text) return 0;
  const char *p = text;
  while (*p && n < max) {
    char *e;
    long v = strtol(p, &e, 0);
    if (e == p) break;
    out[n++] = (int)v;
    p = e;
    if (*p == ',') p++;
  }
  qsort(out, n, sizeof(int), cmp_int);
  return n;
}

bool int_list_has(const int *list, int n, int value) {
  for (int i = 0; i < n; i++) if (list[i] == value) return true;
  return false;
}
