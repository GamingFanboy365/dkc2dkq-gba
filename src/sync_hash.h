// The hash of WRAM that the SYNC logs compare (snesrun, hostrun, hostmini,
// the GBA's SYNC_LOG build). It leaves out what differs between a SNES and
// the recompiled builds without the game's state differing:
// - $2C-$2D, active_frame_counter: counts every NMI, lag frames too;
// - $0100-$01FF, the stack page: at a wait the stack is empty, and the dead
//   bytes under it are what the last interrupts happened to push, which
//   depends on where they landed.
#ifndef SYNC_HASH_H
#define SYNC_HASH_H

#include <stdint.h>

static inline uint32_t sync_hash(const uint8_t *wram) {
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < 0x20000; i++) {
    uint8_t b = (i == 0x2c || i == 0x2d || (i >= 0x100 && i < 0x200)) ? 0 : wram[i];
    h = (h ^ b) * 16777619u;
  }
  return h;
}

#endif
