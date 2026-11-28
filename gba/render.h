// The GBA renderer (gba/render.c).
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

void render_init(void);
// What part of the SNES's 256x224 picture the 240x160 screen shows, from
// the next frame drawn: its lines top to top + lines - 1 (160 to 224 of
// them: more than 160 are squeezed by leaving lines out), columns 8-247.
void render_set_view(int top, int lines);
// Draws the frame the hardware model shows now (after a frame of the
// game's logic), to be put on screen at the next VBlank. Waits if the last
// one isn't on screen yet.
void render_frame(void);
// Waits until the frame drawn last has been on screen for `frames` VBlanks.
void render_hold(int frames);
// VBlanks since the start.
uint32_t render_vblank_count(void);
// Cycles spent drawing, and waiting for the last frame to be shown.
extern uint32_t g_render_cycles, g_idle_cycles;
// From the VBlank interrupt.
void render_vblank(void);
void render_hblank(void);

#endif
