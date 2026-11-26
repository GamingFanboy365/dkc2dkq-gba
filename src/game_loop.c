#include "game_loop.h"

#include <string.h>

#include "cpu65.h"
#include "snes_hw.h"

#ifdef CYCLE_BUDGET
uint32_t g_cycles, g_frame_end;
#endif
static uint32_t g_pc;
static bool g_nmi_since_wait;
static uint16_t g_joy1;

void game_reset(void) {
  hw_reset();
  memset(&cpu, 0, sizeof(cpu));
  cpu.e = 1; cpu.mf = 1; cpu.xf = 1; cpu.i = 1; cpu.s = 0x1ff;
  g_pc = rd16(0xfffc);
#ifdef CYCLE_BUDGET
  g_cycles = 0;
  g_frame_end = MASTER_CYCLES_PER_FRAME;
#endif
  g_nmi_since_wait = true;
}

bool game_frame(void) {
  uint32_t r = run_until_flag(g_pc);
  if (r & PC_STOP) return false;
  uint32_t resume = r & 0xffffff;
  if ((r & PC_YIELD) && g_nmi_since_wait) {
    g_nmi_since_wait = false;
    g_joy1 = on_wait((resume & 0xff0000) | ((resume - 1) & 0xffff));
  }
  // VBlank.
  FRAME_START();
  hw_vblank(g_joy1);
  if (hw_nmi_enabled()) {
    g_hw.in_nmi = 1;
    g_pc = interrupt(0xffea, resume);
    g_nmi_since_wait = true;
  } else {
    g_pc = resume;
  }
  return true;
}
