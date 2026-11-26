// Runs the recompiled game a SNES frame at a time on the hardware model.
#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include <stdint.h>
#include <stdbool.h>

// Platform hooks.
void on_wait(uint32_t wai_pc);   // the game waits for the NMI (once per NMI)
void unknown_entry(uint32_t pc);

void game_reset(void);
// Runs the game until it waits for the NMI or the frame's time is up, then
// starts VBlank with the joypad given and enters the NMI. False if the game
// stopped (an unknown entry, STP).
bool game_frame(uint16_t joy1);

#endif
