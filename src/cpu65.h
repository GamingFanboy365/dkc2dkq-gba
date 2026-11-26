// The runtime the recompiled 65816 code (tools/recomp) runs on.
//
// Each routine of the game is a C function, a "region", taking the address
// it is entered at and returning the address control goes to when it leaves
// the region: a jump elsewhere, a return (RTS, RTL, RTI) or a flag (WAI,
// an interrupt due). Branches inside a region are gotos.
//
// A JSR or JSL pushes its return address on the game's own stack, in WRAM,
// as the SNES does, and calls the target with call_fn/call_pc, which runs
// regions until control comes back to the return address with the stack
// where it was: then the caller carries on natively. If the game plays a
// trick on the stack (pops a return address, resets the stack, returns
// somewhere else), the native frames unwind and the dispatcher carries on
// from wherever the game went, so the stack in WRAM, not C's, decides.
//
// The platform provides rd8/wr8 (the SNES's memory map) and the macros for
// the clock and interrupts (CYC, EVENT_PENDING).
#ifndef CPU65_H
#define CPU65_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint16_t a, x, y, s, d;
  uint8_t db, pb;
  uint8_t n, v, z, c;       // flags, 0 or 1
  uint8_t mf, xf, i, dflag, e;
} Cpu65;

extern Cpu65 cpu;

typedef uint32_t (*RegionFn)(uint32_t entry);

// Flags above the 24-bit address in a region's result.
#define PC_YIELD 0x01000000u  // WAI: wait for an interrupt, then go on at the address
#define PC_INTR  0x02000000u  // an interrupt is due before the address runs
#define PC_STOP  0x04000000u  // STP, BRK, COP or an unknown entry
#define PC_FLAGS 0xff000000u

// ---- platform ----
// rd8, wr8 (the SNES's memory map), rd8_b0, wr8_b0 (bank 0), CYC(n),
// EVENT_PENDING(), ASSUME_MX(m, x, pc).
#include "platform_cpu.h"

static inline __attribute__((always_inline)) uint16_t rd16(uint32_t a) {
#ifdef WRAM_DIRECT
  // A constant address in WRAM (7E/7F, or 00-3F/80-BF below $2000), even
  if (__builtin_constant_p(a) && !(a & 1)) {
    if ((a >> 17) == 0x3f) return *(const uint16_t *)(g_wram + (a & 0x1ffff));
    if (!(a & 0x400000) && (a & 0xffff) < 0x2000) return *(const uint16_t *)(g_wram + (a & 0xffff));
  }
#endif
  return rd8(a) | rd8((a + 1) & 0xffffff) << 8;
}
static inline uint32_t rd24(uint32_t a) {
  return rd8(a) | rd8((a + 1) & 0xffffff) << 8 | (uint32_t)rd8((a + 2) & 0xffffff) << 16;
}
static inline __attribute__((always_inline)) void wr16(uint32_t a, uint16_t v) {
#ifdef WRAM_DIRECT
  if (__builtin_constant_p(a) && !(a & 1)) {
    if ((a >> 17) == 0x3f) { *(uint16_t *)(g_wram + (a & 0x1ffff)) = v; return; }
    if (!(a & 0x400000) && (a & 0xffff) < 0x2000) { *(uint16_t *)(g_wram + (a & 0xffff)) = v; return; }
  }
#endif
  wr8(a, (uint8_t)v);
  wr8((a + 1) & 0xffffff, v >> 8);
}
// Bank 0's 16-bit accesses wrap within it. At an even address known when
// compiling, in WRAM, where the platform keeps WRAM itself (WRAM_DIRECT):
// one halfword access.
static inline __attribute__((always_inline)) uint16_t rd16_b0(uint16_t a) {
#ifdef WRAM_DIRECT
  if (__builtin_constant_p(a) && !(a & 1) && a < 0x2000) return *(const uint16_t *)(g_wram + a);
#endif
  return rd8_b0(a) | rd8_b0((uint16_t)(a + 1)) << 8;
}
static inline __attribute__((always_inline)) void wr16_b0(uint16_t a, uint16_t v) {
#ifdef WRAM_DIRECT
  if (__builtin_constant_p(a) && !(a & 1) && a < 0x2000) { *(uint16_t *)(g_wram + a) = v; return; }
#endif
  wr8_b0(a, (uint8_t)v);
  wr8_b0((uint16_t)(a + 1), v >> 8);
}
static inline uint32_t rd24_b0(uint16_t a) { return rd16_b0(a) | (uint32_t)rd8_b0((uint16_t)(a + 2)) << 16; }

static inline void push8(uint8_t v) { wr8_b0(cpu.s, v); cpu.s--; }
static inline void push16(uint16_t v) { wr8_b0(cpu.s, v >> 8); wr8_b0((uint16_t)(cpu.s - 1), (uint8_t)v); cpu.s -= 2; }
static inline uint8_t pull8(void) { cpu.s++; return rd8_b0(cpu.s); }
static inline uint16_t pull16(void) {
  uint16_t lo = rd8_b0((uint16_t)(cpu.s + 1));
  uint16_t hi = rd8_b0((uint16_t)(cpu.s + 2));
  cpu.s += 2;
  return lo | hi << 8;
}

// ---- dispatch (src/runtime.c; the entry table is generated) ----
extern uint32_t g_unwind;
RegionFn lookup_entry(uint32_t pc);      // NULL if pc isn't a known entry
uint32_t dispatch(uint32_t pc);
bool call_fn(RegionFn fn, uint32_t target, uint32_t ret, uint16_t s0);
// After a call's target returned pc: true if it's back at ret with the stack
// at s0, by way of the dispatcher if need be (see cpu65.h's top).
bool finish_call(uint32_t pc, uint32_t ret, uint16_t s0);
bool call_pc(uint32_t target, uint32_t ret, uint16_t s0);
uint32_t run_until_flag(uint32_t pc);
uint32_t interrupt(uint16_t vector_addr, uint32_t resume_pc);

// ---- what the generated code uses ----
// The registers and flags, and D and DB, are locals while a region runs
// (so the compiler keeps them in registers: stores to WRAM, byte stores,
// could change cpu's fields as far as it knows): saved before anything else
// may look at them (a call, a return, an interrupt), reloaded after a call.
#define LOAD() uint16_t A = cpu.a, X = cpu.x, Y = cpu.y, D = cpu.d; uint8_t DB = cpu.db; \
    uint8_t fN = cpu.n, fV = cpu.v, fZ = cpu.z, fC = cpu.c
#define RELOAD() (A = cpu.a, X = cpu.x, Y = cpu.y, D = cpu.d, DB = cpu.db, fN = cpu.n, fV = cpu.v, fZ = cpu.z, \
    fC = cpu.c)
#define SAVE() (cpu.a = A, cpu.x = X, cpu.y = Y, cpu.d = D, cpu.db = DB, cpu.n = fN, cpu.v = fV, cpu.z = fZ, \
    cpu.c = fC)

#define NZ8(r) (fN = ((r) >> 7) & 1, fZ = (uint8_t)(r) == 0)
#define NZ16(r) (fN = ((r) >> 15) & 1, fZ = (uint16_t)(r) == 0)
#define DP(o) ((uint16_t)(D + (o)))
#define DBA(a) (((uint32_t)DB << 16) | (uint16_t)(a))
// The same with D (DB) known: where the trace saw only one value of it at
// an instruction, the instruction is emitted for that value (its address a
// constant) and for any other, by a test of D (DB).
#define KDP(k, o) ((uint16_t)((k) + (o)))
#define KDB(k, a) (((uint32_t)(k) << 16) | (uint16_t)(a))
#define PBA(a) (((uint32_t)cpu.pb << 16) | (uint16_t)(a))
#define L24(a) ((a) & 0xffffff)
#define SR(o) ((uint16_t)(cpu.s + (o)))

#define GETP() (uint8_t)(fN << 7 | fV << 6 | cpu.mf << 5 | cpu.xf << 4 | cpu.dflag << 3 | cpu.i << 2 | fZ << 1 | fC)
#define SETP(p) do { uint8_t _p = (p); fN = _p >> 7; fV = (_p >> 6) & 1; cpu.mf = (_p >> 5) & 1; \
    cpu.xf = (_p >> 4) & 1; cpu.dflag = (_p >> 3) & 1; cpu.i = (_p >> 2) & 1; fZ = (_p >> 1) & 1; fC = _p & 1; \
    if (cpu.e) cpu.mf = cpu.xf = 1; if (cpu.xf) { X &= 0xff; Y &= 0xff; } } while (0)

// Polls for an interrupt before a backward branch.
#define POLL(target16) do { if (EVENT_PENDING()) { SAVE(); return PC_INTR | PBA(target16); } } while (0)

// ADC and SBC, binary and decimal; the decimal steps are LakeSnes's
// (third_party/lakesnes/cpu.c), the reference these are checked against.
static inline uint8_t adc8(uint8_t a, uint8_t v, uint8_t *c, uint8_t *ov) {
  int r;
  if (cpu.dflag) {
    r = (a & 0xf) + (v & 0xf) + *c;
    if (r > 0x9) r = ((r + 0x6) & 0xf) + 0x10;
    r = (a & 0xf0) + (v & 0xf0) + r;
  } else {
    r = a + v + *c;
  }
  *ov = (a & 0x80) == (v & 0x80) && (v & 0x80) != (r & 0x80);
  if (cpu.dflag && r > 0x9f) r += 0x60;
  *c = r > 0xff;
  return (uint8_t)r;
}
static inline uint16_t adc16(uint16_t a, uint16_t v, uint8_t *c, uint8_t *ov) {
  int r;
  if (cpu.dflag) {
    r = (a & 0xf) + (v & 0xf) + *c;
    if (r > 0x9) r = ((r + 0x6) & 0xf) + 0x10;
    r = (a & 0xf0) + (v & 0xf0) + r;
    if (r > 0x9f) r = ((r + 0x60) & 0xff) + 0x100;
    r = (a & 0xf00) + (v & 0xf00) + r;
    if (r > 0x9ff) r = ((r + 0x600) & 0xfff) + 0x1000;
    r = (a & 0xf000) + (v & 0xf000) + r;
  } else {
    r = a + v + *c;
  }
  *ov = (a & 0x8000) == (v & 0x8000) && (v & 0x8000) != (r & 0x8000);
  if (cpu.dflag && r > 0x9fff) r += 0x6000;
  *c = r > 0xffff;
  return (uint16_t)r;
}
static inline uint8_t sbc8(uint8_t a, uint8_t v, uint8_t *c, uint8_t *ov) {
  v ^= 0xff;
  int r;
  if (cpu.dflag) {
    r = (a & 0xf) + (v & 0xf) + *c;
    if (r < 0x10) r = (r - 0x6) & ((r - 0x6 < 0) ? 0xf : 0x1f);
    r = (a & 0xf0) + (v & 0xf0) + r;
  } else {
    r = a + v + *c;
  }
  *ov = (a & 0x80) == (v & 0x80) && (v & 0x80) != (r & 0x80);
  if (cpu.dflag && r < 0x100) r -= 0x60;
  *c = r > 0xff;
  return (uint8_t)r;
}
static inline uint16_t sbc16(uint16_t a, uint16_t v, uint8_t *c, uint8_t *ov) {
  v ^= 0xffff;
  int r;
  if (cpu.dflag) {
    r = (a & 0xf) + (v & 0xf) + *c;
    if (r < 0x10) r = (r - 0x6) & ((r - 0x6 < 0) ? 0xf : 0x1f);
    r = (a & 0xf0) + (v & 0xf0) + r;
    if (r < 0x100) r = (r - 0x60) & ((r - 0x60 < 0) ? 0xff : 0x1ff);
    r = (a & 0xf00) + (v & 0xf00) + r;
    if (r < 0x1000) r = (r - 0x600) & ((r - 0x600 < 0) ? 0xfff : 0x1fff);
    r = (a & 0xf000) + (v & 0xf000) + r;
  } else {
    r = a + v + *c;
  }
  *ov = (a & 0x8000) == (v & 0x8000) && (v & 0x8000) != (r & 0x8000);
  if (cpu.dflag && r < 0x10000) r -= 0x6000;
  *c = r > 0xffff;
  return (uint16_t)r;
}

#endif
