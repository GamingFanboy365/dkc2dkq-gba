// The GBA renderer (gba/render.c).
#ifndef RENDER_H
#define RENDER_H

void render_init(void);
// Draws the frame the hardware model shows now (after a frame of the
// game's logic), to be put on screen at the next VBlank. Waits if the last
// one isn't on screen yet.
void render_frame(void);
// Waits until the frame drawn last has been on screen for `frames` VBlanks.
void render_hold(int frames);
// From the VBlank interrupt.
void render_vblank(void);

#endif
