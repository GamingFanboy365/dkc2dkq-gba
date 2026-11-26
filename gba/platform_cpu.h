// The GBA's clock and checks for the recompiled code (src/cpu65.h).
//
// No cycle counting: the game's logic runs until it waits for the NMI. If a
// frame of it takes more than a few of the GBA's frames, the VBlank
// interrupt raises g_lag_event and the NMI is taken where the logic is, as
// on a SNES in a lag frame (the game's NMI handler is ready for that).
#ifndef PLATFORM_CPU_H
#define PLATFORM_CPU_H

#include <stdint.h>

#include "snes_mem.h"

// Hot code goes to IWRAM as ARM.
#define HOT_CODE __attribute__((section(".iwram"), target("arm"), noinline))

extern volatile uint8_t g_lag_event;
#define CYC(n) ((void)0)
#define EVENT_PENDING() (g_lag_event)
#define FRAME_START() (g_lag_event = 0, g_frame_vblanks = 0)
extern volatile uint8_t g_frame_vblanks;
#define LAG_VBLANKS 6
#define ASSUME_MX(m, x, pc) ((void)0)

#endif
