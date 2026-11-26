// Dispatch for the recompiled code: see cpu65.h.
#include "cpu65.h"

Cpu65 cpu;
uint32_t g_unwind;

// Generated (gen/entries.c): every address a region can be entered at,
// sorted, with banks 00-3F given as their mirrors 80-BF.
extern const uint32_t g_entry_pcs[];
extern const RegionFn g_entry_fns[];
extern const uint32_t g_num_entries;

void unknown_entry(uint32_t pc);  // platform: report it

static inline uint32_t canonical(uint32_t pc) {
  return (pc >> 16) < 0x40 ? pc | 0x800000 : pc;
}

RegionFn lookup_entry(uint32_t pc) {
  uint32_t key = canonical(pc);
  uint32_t lo = 0, hi = g_num_entries;
  while (lo < hi) {
    uint32_t mid = (lo + hi) >> 1;
    if (g_entry_pcs[mid] < key) lo = mid + 1;
    else hi = mid;
  }
  return lo < g_num_entries && g_entry_pcs[lo] == key ? g_entry_fns[lo] : 0;
}

uint32_t dispatch(uint32_t pc) {
  RegionFn fn = lookup_entry(pc);
  if (!fn) {
    unknown_entry(pc);
    return PC_STOP | pc;
  }
  cpu.pb = pc >> 16;
  return fn(pc);
}

// Runs the call's target until it comes back to `ret` with the stack at s0
// (the stack pointer before the return address was pushed). Returns false,
// with where the game went in g_unwind, if it leaves the call another way:
// a flag, or a stack popped above s0.
static bool finish_call(uint32_t pc, uint32_t ret, uint16_t s0) {
  for (;;) {
    if (pc == ret && cpu.s == s0) {
      cpu.pb = ret >> 16;
      return true;
    }
    if ((pc & PC_FLAGS) || cpu.s > s0) {
      g_unwind = pc;
      return false;
    }
    if (EVENT_PENDING()) {
      g_unwind = PC_INTR | pc;
      return false;
    }
    pc = dispatch(pc);
  }
}

bool call_fn(RegionFn fn, uint32_t target, uint32_t ret, uint16_t s0) {
  cpu.pb = target >> 16;
  return finish_call(fn(target), ret, s0);
}

bool call_pc(uint32_t target, uint32_t ret, uint16_t s0) {
  return finish_call(dispatch(target), ret, s0);
}

uint32_t run_until_flag(uint32_t pc) {
  for (;;) {
    if (pc & PC_FLAGS) return pc;
    if (EVENT_PENDING()) return PC_INTR | pc;
    pc = dispatch(pc);
  }
}

// Enters an interrupt as the 65816 does; returns the handler's address, from
// the vector at vector_addr in bank 0.
uint32_t interrupt(uint16_t vector_addr, uint32_t resume_pc) {
  if (!cpu.e) push8(resume_pc >> 16);
  push16((uint16_t)resume_pc);
  uint8_t p = (uint8_t)(cpu.n << 7 | cpu.v << 6 | cpu.mf << 5 | cpu.xf << 4 | cpu.dflag << 3 |
                        cpu.i << 2 | cpu.z << 1 | cpu.c);
  push8(cpu.e ? (p & ~0x10) : p);
  cpu.i = 1;
  cpu.dflag = 0;
  cpu.pb = 0;
  return rd16(vector_addr);
}
