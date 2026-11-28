#!/usr/bin/env python3
"""Static recompiler: the disassembly's 65816 code, as C.

  recomp.py --rom baserom.sfc --asar ASAR --out gen [--trace FILE ...]

Where each instruction is and what it is comes from the disassembly (asar is
asked, asm_probe.py) and the player's ROM (the bytes); the generated C is
the game's code, so it's built from the player's ROM and never committed.

What the recompiler has to know that the bytes don't say:

- The width of A and of X/Y (the M and X flags) at each instruction. An
  immediate operand's length says it; elsewhere a flow analysis carries it
  from REP/SEP, and a trace of the original running on LakeSnes (snesrun
  TRACE=) fills in what the analysis can't decide (after PLP, at entries
  reached through pointers). Where neither decides, the C tests the flag at
  run time. Where the trace or a guess decided, ASSUME_MX checks it on the
  host build.
- Where code can be entered from outside its routine: every label, every
  return address, the targets of jumps through pointers seen in the trace.

See src/cpu65.h for the runtime and the shape of the generated code.
"""
import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from asm_probe import probe  # noqa: E402
from opcodes import OPCODES, OPERAND_SIZE, M_OPS, X_OPS, CYCLES, same_mnemonic  # noqa: E402

# Width sets: bit 0 = 8-bit possible, bit 1 = 16-bit possible.
W8, W16, WANY = 1, 2, 3
NO_FALLTHROUGH = {"RTS", "RTL", "RTI", "JMP", "JML", "BRA", "BRL", "STP", "BRK", "COP"}
BRANCHES = {"BPL": "!fN", "BMI": "fN", "BVC": "!fV", "BVS": "fV", "BCC": "!fC", "BCS": "fC",
            "BNE": "!fZ", "BEQ": "fZ", "BRA": None, "BRL": None}
MAX_REGION = 1500


# Routines with native versions (src/native.c): their recompiled code is
# emitted as R_xxxxxx_recomp, which the native function falls back on.
NATIVE = {0xB59F40, 0x80E580, 0xB5A8DA, 0xB5F5E1, 0xBB8000, 0x8088BA}


def rom_offset(addr):
    bank, low = addr >> 16, addr & 0xFFFF
    if bank >= 0xC0 or 0x40 <= bank < 0x7E:
        return addr & 0x3FFFFF
    if low >= 0x8000:
        return (bank & 0x3F) << 16 | low
    raise ValueError(f"no ROM at {addr:06X}")


def canonical(addr):
    return addr | 0x800000 if (addr >> 16) < 0x40 else addr


class Insn:
    __slots__ = ("addr", "op", "mnem", "mode", "length", "operand", "marker", "file", "line",
                 "m_in", "x_in", "assumed", "region", "m_pred", "x_pred")

    def __init__(self, addr, op, mnem, mode, file, line, marker):
        self.addr, self.op, self.mnem, self.mode = addr, op, mnem, mode
        self.file, self.line, self.marker = file, line, marker
        self.length = 1 + OPERAND_SIZE[mode]
        self.operand = 0
        self.m_in = self.x_in = 0
        self.m_pred = self.x_pred = 0   # the widths its predecessors give it
        self.assumed = False
        self.region = None

    @property
    def next(self):
        return (self.addr & 0xFF0000) | ((self.addr + self.length) & 0xFFFF)


# Per traced instruction, the one value of DB (D) the trace saw there, or
# None if it saw more (ea_expr then emits DBK/DPK: see src/cpu65.h).
TRACE_DB, TRACE_DP = {}, {}


def load_traces(paths):
    flags, edges = collections.defaultdict(int), set()
    for path in paths:
        for line in open(path):
            p = line.split()
            if p[0] == "E":
                edges.add((canonical(int(p[1], 16)), canonical(int(p[2], 16))))
            else:
                a = canonical(int(p[0], 16))
                flags[a] |= int(p[1], 16)
                for k, d in ((2, TRACE_DB), (3, TRACE_DP)):
                    if len(p) <= k:
                        continue
                    v = None if p[k].endswith("*") else int(p[k], 16)
                    d[a] = v if d.get(a, v) == v else None
    return flags, edges


def traced_widths(f):
    m = (W8 if f & 1 else 0) | (W16 if f & 2 else 0)
    x = (W8 if f & 4 else 0) | (W16 if f & 8 else 0)
    return m, x


def decode(code, rom, tflags):
    insns = {}
    order = sorted(code)
    for k, (addr, file, line, src_mnem, marker) in enumerate(order):
        op = rom[rom_offset(addr)]
        mnem, mode = OPCODES[op]
        if not same_mnemonic(src_mnem, mnem):
            raise SystemExit(f"{file}:{line}: {src_mnem} in the source, {mnem} in the ROM at {addr:06X}")
        i = Insn(addr, op, mnem, mode, file, line, marker)
        if mode in ("immm", "immx"):
            nxt = order[k + 1][0] if k + 1 < len(order) else None
            if nxt is not None and nxt - addr in (2, 3):
                i.length = nxt - addr
            else:
                m, x = traced_widths(tflags.get(addr, 0))
                w = m if mode == "immm" else x
                if w not in (W8, W16):
                    raise SystemExit(f"{file}:{line}: can't tell the immediate's size at {addr:06X}")
                i.length = 2 if w == W8 else 3
        off = rom_offset(addr)
        i.operand = int.from_bytes(rom[off + 1:off + i.length], "little")
        insns[addr] = i
    return insns


def resync(insns, rom, tflags):
    """Corrects the disassembly where the trace shows it decoded the bytes
    with the wrong M/X width: an instruction executed where the disassembly
    has none starting, or an immediate executed at another width than the
    disassembly's length says (e.g. $B5A262, AND #$1F / STA $56 / TXA / ...,
    which the disassembly reads as AND #$851F / LSR $8A,x / AND #$AA03: the
    same bytes, so a byte-exact rebuild can't tell). Re-decodes from there,
    by the trace's widths, until it meets the disassembly's instructions
    again. Returns the addresses re-decoded."""
    starts = sorted(insns)
    import bisect
    fixed = []

    def covering(a):
        k = bisect.bisect_right(starts, a) - 1
        while k >= 0:
            s = starts[k]
            if s in insns:
                return s if s < a < s + insns[s].length else None
            k -= 1
        return None

    def conflicts(a):
        i = insns.get(a)
        if i is None:
            return True
        m, x = traced_widths(tflags[a])
        w = m if i.mode == "immm" else x if i.mode == "immx" else None
        return w in (W8, W16) and i.length != (2 if w == W8 else 3)

    for t in sorted(tflags):
        if t not in insns and covering(t) is None:
            continue  # not in the disassembly's code at all (reported by main)
        if not conflicts(t):
            continue
        a = t
        m, x = traced_widths(tflags[t])
        template = insns.get(a) or insns.get(covering(a))
        while True:
            if a in tflags:
                tm, tx = traced_widths(tflags[a])
                m = tm if tm in (W8, W16) else m
                x = tx if tx in (W8, W16) else x
            op = rom[rom_offset(a)]
            mnem, mode = OPCODES[op]
            i = Insn(a, op, mnem, mode, template.file, template.line, "|")
            if mode == "immm":
                if m not in (W8, W16):
                    break
                i.length = 2 if m == W8 else 3
            if mode == "immx":
                if x not in (W8, W16):
                    break
                i.length = 2 if x == W8 else 3
            off = rom_offset(a)
            i.operand = int.from_bytes(rom[off + 1:off + i.length], "little")
            c = covering(a)
            if c is not None:
                del insns[c]
            for s in list(insns):
                if a < s < a + i.length:
                    del insns[s]
            insns[a] = i
            fixed.append(a)
            m, x = transfer(i, m, x)
            nxt = a + i.length
            if mnem in NO_FALLTHROUGH or (nxt in insns and nxt not in tflags) or \
                    (nxt in insns and not conflicts(nxt)):
                break
            a = nxt
        starts = sorted(insns)
    return fixed


def width_conflicts(insns, tflags):
    """Immediates no trace ran whose length, from the disassembly, says
    another width than the analysis carried to them from all their
    predecessors (the disassembly decoded those bytes at the wrong width,
    and nothing has shown it): [(address, m, x)], the widths to decode by."""
    out = []
    for a, i in sorted(insns.items()):
        if a in tflags:
            continue
        if i.mode == "immm" and i.m_pred in (W8, W16) and i.m_pred != (W8 if i.length == 2 else W16):
            out.append((a, i.m_pred, i.x_pred))
        elif i.mode == "immx" and i.x_pred in (W8, W16) and i.x_pred != (W8 if i.length == 2 else W16):
            out.append((a, i.m_pred, i.x_pred))
    return out


def redecode(insns, rom, a, m, x):
    """Decodes the bytes from a with widths m, x (carried by REP/SEP),
    replacing the disassembly's instructions there, until an instruction
    ends where one of the disassembly's begins. Returns the addresses."""
    template = insns[a]
    fixed = []
    while True:
        op = rom[rom_offset(a)]
        mnem, mode = OPCODES[op]
        i = Insn(a, op, mnem, mode, template.file, template.line, "|")
        if mode == "immm":
            if m not in (W8, W16):
                break
            i.length = 2 if m == W8 else 3
        if mode == "immx":
            if x not in (W8, W16):
                break
            i.length = 2 if x == W8 else 3
        off = rom_offset(a)
        i.operand = int.from_bytes(rom[off + 1:off + i.length], "little")
        for s in [s for s in insns if a <= s < a + i.length]:
            del insns[s]
        insns[a] = i
        fixed.append(a)
        m, x = transfer(i, m, x)
        nxt = a + i.length
        if mnem in NO_FALLTHROUGH or nxt in insns:
            break
        a = nxt
    return fixed


def rel_target(i):
    if i.mode == "rel":
        d = i.operand - 0x100 if i.operand & 0x80 else i.operand
    else:
        d = i.operand - 0x10000 if i.operand & 0x8000 else i.operand
    return (i.addr & 0xFF0000) | ((i.addr + i.length + d) & 0xFFFF)


def static_target(i):
    """The address a direct JMP/JML/JSR/JSL or branch goes to, or None."""
    if i.mnem in BRANCHES:
        return rel_target(i)
    if i.mnem in ("JMP", "JSR") and i.mode == "abs":
        return (i.addr & 0xFF0000) | i.operand
    if i.mnem in ("JML", "JSL") and i.mode == "absl":
        return canonical(i.operand)
    return None


def successors(i, insns, dyn):
    """Control-flow successors: (address, kind) with kind 'flow' or 'call'
    (a call's target) or 'return' (a call's return point)."""
    out = []
    if i.mnem not in NO_FALLTHROUGH:
        out.append((i.next, "return" if i.mnem in ("JSR", "JSL") else "flow"))
    t = static_target(i)
    if t is not None:
        out.append((t, "call" if i.mnem in ("JSR", "JSL") else "flow"))
    if i.mode in ("absi", "absix", "absil"):
        for to in dyn.get(i.addr, ()):
            out.append((to, "call" if i.mnem == "JSR" else "flow"))
    return [(a, k) for a, k in out if a in insns]


def transfer(i, m, x):
    """M/X widths after instruction i, given those before it."""
    if i.mnem == "REP":
        if i.operand & 0x20:
            m = W16
        if i.operand & 0x10:
            x = W16
    elif i.mnem == "SEP":
        if i.operand & 0x20:
            m = W8
        if i.operand & 0x10:
            x = W8
    elif i.mnem in ("PLP", "RTI", "XCE"):
        m = x = WANY
    return m, x


def infer_widths(insns, tflags, dyn):
    """Forward analysis of the M/X widths; sets m_in, x_in, assumed."""
    preds_in = {a: [0, 0] for a in insns}
    for a, f in tflags.items():
        if a in preds_in:
            m, x = traced_widths(f)
            preds_in[a][0] |= m
            preds_in[a][1] |= x
    has_pred = set()
    for i in insns.values():
        for a, _ in successors(i, insns, dyn):
            has_pred.add(a)
    for a in insns:
        if a not in has_pred and a not in tflags:
            preds_in[a] = [WANY, WANY]
    work = collections.deque(sorted(insns))
    queued = set(insns)
    while work:
        a = work.popleft()
        queued.discard(a)
        i = insns[a]
        m, x = preds_in[a]
        tm, tx = traced_widths(tflags.get(a, 0))
        # The trace decides where the analysis can't; where they disagree, both.
        m = tm if (m == WANY and tm) else (m | tm)
        x = tx if (x == WANY and tx) else (x | tx)
        i.m_pred, i.x_pred = m, x
        if i.mode == "immm":
            m = W8 if i.length == 2 else W16
        if i.mode == "immx":
            x = W8 if i.length == 2 else W16
        if (m, x) == (i.m_in, i.x_in):
            continue
        i.m_in, i.x_in = m, x
        om, ox = transfer(i, m, x)
        for s, kind in successors(i, insns, dyn):
            pm, px = preds_in[s]
            # A call's return point: assume the callee leaves M/X as they were.
            nm, nx = (m, x) if kind == "return" else (om, ox)
            if (pm | nm, px | nx) != (pm, px):
                preds_in[s] = [pm | nm, px | nx]
                if s not in queued:
                    work.append(s)
                    queued.add(s)
    for i in insns.values():
        if not i.m_in:
            i.m_in = WANY
        if not i.x_in:
            i.x_in = WANY
    # Mark what a guess or the trace decided, to check on the host build:
    # return points, and traced instructions the analysis alone left open.
    for i in insns.values():
        if i.mnem in ("JSR", "JSL") and i.next in insns:
            insns[i.next].assumed = True
    for a, f in tflags.items():
        if a in insns:
            insns[a].assumed = True


# At most this many instructions in a function: bigger ones (a bank's
# routines merged by their jumps, with hundreds of entries) spill so much
# that one frame took 2.6 KB of the GBA's 3.8 KB stack.
MAX_FUNCTION = 1000


def make_regions(insns):
    """The C functions: first the disassembly's routines (its routine
    markers, and wherever the code isn't contiguous), then merged where
    control passes between them by a branch, a JMP or falling through (in
    the same bank): every such transfer is then a goto, and only calls,
    returns and jumps through pointers leave a function."""
    routines, cur, prev = [], None, None
    for a in sorted(insns):
        i = insns[a]
        start = (cur is None or prev.next != a or (a >> 16) != (prev.addr >> 16)
                 or i.marker in ("\\", ">") or prev.marker in ("/", ">")
                 or (len(cur) >= MAX_REGION and i.file == prev.file))
        if start:
            cur = []
            routines.append(cur)
        cur.append(i)
        i.region = len(routines) - 1
        prev = i

    parent = list(range(len(routines)))
    size = [len(r) for r in routines]

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for i in insns.values():
        targets = []
        t = static_target(i)
        if t is not None and i.mnem not in ("JSR", "JSL"):
            targets.append(t)
        if i.mnem not in NO_FALLTHROUGH and i.mnem not in ("JSR", "JSL", "WAI"):
            targets.append(i.next)
        for t in targets:
            if t in insns and (t >> 16) == (i.addr >> 16):
                a, b = find(i.region), find(insns[t].region)
                if a != b and size[a] + size[b] <= MAX_FUNCTION:
                    parent[a] = b
                    size[b] += size[a]

    groups = collections.defaultdict(list)
    for r in range(len(routines)):
        groups[find(r)].append(r)
    regions = []
    for g in sorted(groups.values(), key=lambda rs: routines[rs[0]][0].addr):
        merged = sorted((i for r in g for i in routines[r]), key=lambda i: i.addr)
        for i in merged:
            i.region = len(regions)
        regions.append(merged)
    return regions


# ---------------------------------------------------------------- emission

def hx(v, n=4):
    return f"0x{v:0{n}x}"


# Addressing modes whose address is a constant when D (DB) is: the
# instruction is emitted twice, for the value the trace saw and for any other.
DP_CONST_MODES = {"dp", "dpi", "dpiy", "dpil", "dpily"}
DB_CONST_MODES = {"abs", "absx", "absy", "dpi", "dpiy"}


def const_cases(i):
    """{'D': k, 'DB': k} for the registers the instruction's address can be
    made constant by, from the trace; empty if none."""
    out = {}
    if i.mode in DP_CONST_MODES and TRACE_DP.get(i.addr) is not None:
        out["D"] = TRACE_DP[i.addr]
    if i.mode in DB_CONST_MODES and TRACE_DB.get(i.addr) is not None:
        out["DB"] = TRACE_DB[i.addr]
    return out


CONSTS = {}   # while emitting the constant case: {'D': k, 'DB': k}


def ea_expr(i):
    e = ea_expr_generic(i)
    if "D" in CONSTS:
        e = e.replace("DP(", f"KDP({hx(CONSTS['D'])}, ")
    if "DB" in CONSTS:
        e = e.replace("DBA(", f"KDB({hx(CONSTS['DB'], 2)}, ")
    return e


def lx_parts(i):
    """(base, index) of an indexed absolute or long access, for rd*_lx and
    wr*_lx (src/cpu65.h); None for other modes."""
    o, m = i.operand, i.mode
    if m in ("absx", "absy"):
        base = f"DBA({hx(o)})"
        if "DB" in CONSTS:
            base = f"KDB({hx(CONSTS['DB'], 2)}, {hx(o)})"
        return base, "X" if m == "absx" else "Y"
    if m == "abslx":
        return hx(o, 6), "X"
    return None


def ea_expr_generic(i):
    o, m = i.operand, i.mode
    if m == "dp": return f"DP({hx(o, 2)})"
    if m == "dpx": return f"DP({hx(o, 2)} + X)"
    if m == "dpy": return f"DP({hx(o, 2)} + Y)"
    if m == "dpi": return f"DBA(rd16_b0(DP({hx(o, 2)})))"
    if m == "dpix": return f"DBA(rd16_b0(DP({hx(o, 2)} + X)))"
    if m == "dpiy": return f"L24(DBA(rd16_b0(DP({hx(o, 2)}))) + Y)"
    if m == "dpil": return f"rd24_b0(DP({hx(o, 2)}))"
    if m == "dpily": return f"L24(rd24_b0(DP({hx(o, 2)})) + Y)"
    if m == "abs": return f"DBA({hx(o)})"
    if m == "absx": return f"L24(DBA({hx(o)}) + X)"
    if m == "absy": return f"L24(DBA({hx(o)}) + Y)"
    if m == "absl": return hx(o, 6)
    if m == "abslx": return f"L24({hx(o, 6)} + X)"
    if m == "sr": return f"SR({hx(o, 2)})"
    if m == "sriy": return f"L24(DBA(rd16_b0(SR({hx(o, 2)}))) + Y)"
    raise ValueError(m)


def emit_op(i, w):
    """C for instruction i with operation width w (8 or 16, or None if the
    instruction has no width)."""
    mn, mode = i.mnem, i.mode
    imm = mode in ("immm", "immx")
    # The direct page and the stack are in bank 0.
    b0 = "_b0" if mode in ("dp", "dpx", "dpy", "sr") else ""
    rd = f"rd{w}{b0}" if w else None
    wr = f"wr{w}{b0}" if w else None
    u = f"uint{w}_t" if w else None
    nz = f"NZ{w}" if w else None
    val = hx(i.operand, 4 if w == 16 else 2) if imm else (f"{rd}({ea_expr(i)})" if w and mode not in ("imp", "acc") else None)
    lx = lx_parts(i) if w else None
    if lx and not imm:
        val = f"rd{w}_lx({lx[0]}, {lx[1]})"
    lo = "(uint8_t)A"

    def set_a(expr):  # A's low byte or all of it
        return f"A = (A & 0xff00) | (uint8_t)({expr});" if w == 8 else f"A = {expr};"

    if mn == "LDA":
        return [f"{{ {u} t = {val}; {set_a('t')} {nz}(t); }}"]
    if mn in ("LDX", "LDY"):
        r = mn[2]
        return [f"{{ {u} t = {val}; {r} = t; {nz}(t); }}"]
    if mn in ("STA", "STX", "STY", "STZ"):
        src = {"STA": "A", "STX": "X", "STY": "Y", "STZ": "0"}[mn]
        if lx:
            return [f"wr{w}_lx({lx[0]}, {lx[1]}, ({u}){src});"]
        return [f"{wr}({ea_expr(i)}, ({u}){src});"]
    if mn in ("ORA", "AND", "EOR"):
        c = {"ORA": "|", "AND": "&", "EOR": "^"}[mn]
        a = lo if w == 8 else "A"
        return [f"{{ {u} t = ({u})({a} {c} {val}); {set_a('t')} {nz}(t); }}"]
    if mn in ("ADC", "SBC"):
        f = mn.lower() + str(w)
        a = lo if w == 8 else "A"
        return [f"{{ {u} t = {f}({a}, {val}, &fC, &fV); {set_a('t')} {nz}(t); }}"]
    if mn in ("CMP", "CPX", "CPY"):
        r = {"CMP": "A", "CPX": "X", "CPY": "Y"}[mn]
        return [f"{{ {u} v = {val}; {u} r = ({u}){r}; fC = r >= v; {nz}(({u})(r - v)); }}"]
    if mn == "BIT":
        a = lo if w == 8 else "A"
        if imm:
            return [f"fZ = ({u})({a} & {val}) == 0;"]
        top = 7 if w == 8 else 15
        return [f"{{ {u} v = {val}; fN = (v >> {top}) & 1; fV = (v >> {top - 1}) & 1; fZ = ({u})({a} & v) == 0; }}"]
    if mn in ("INC", "DEC", "ASL", "LSR", "ROL", "ROR"):
        top = 7 if w == 8 else 15
        body = {
            "INC": "t++;", "DEC": "t--;",
            "ASL": f"fC = (t >> {top}) & 1; t <<= 1;",
            "LSR": "fC = t & 1; t >>= 1;",
            "ROL": f"{{ uint8_t c = fC; fC = (t >> {top}) & 1; t = ({u})(t << 1 | c); }}",
            "ROR": f"{{ uint8_t c = fC; fC = t & 1; t = ({u})(t >> 1 | c << {top}); }}",
        }[mn]
        if mode == "acc":
            a = lo if w == 8 else "A"
            return [f"{{ {u} t = {a}; {body} {set_a('t')} {nz}(t); }}"]
        return [f"{{ uint32_t ea = {ea_expr(i)}; {u} t = {rd}(ea); {body} {wr}(ea, t); {nz}(t); }}"]
    if mn in ("TSB", "TRB"):
        a = lo if w == 8 else "A"
        nv = f"v | {a}" if mn == "TSB" else f"v & ~{a}"
        return [f"{{ uint32_t ea = {ea_expr(i)}; {u} v = {rd}(ea); fZ = ({u})({a} & v) == 0; {wr}(ea, ({u})({nv})); }}"]
    if mn in ("INX", "INY", "DEX", "DEY"):
        r = mn[2]
        op = "+" if mn[0] == "I" else "-"
        return [f"{r} = ({u})({r} {op} 1); {nz}({r});"]
    if mn in ("TAX", "TAY"):
        r = mn[2]
        return [f"{r} = ({u})A; {nz}({r});"]
    if mn in ("TXA", "TYA"):
        r = mn[1]
        return [f"{set_a(r)} {nz}(({u})A);"]
    if mn in ("TXY", "TYX"):
        s, d = mn[1], mn[2]
        return [f"{d} = {s}; {nz}({d});"]
    if mn == "TSX":
        return [f"X = ({u})cpu.s; {nz}(X);"]
    if mn == "TXS":
        return ["cpu.s = cpu.e ? (0x100 | (X & 0xff)) : X;"]
    if mn == "TCD":
        return ["D = A; NZ16(A);"]
    if mn == "TDC":
        return ["A = D; NZ16(A);"]
    if mn == "TCS":
        return ["cpu.s = cpu.e ? (0x100 | (A & 0xff)) : A;"]
    if mn == "TSC":
        return ["A = cpu.s; NZ16(A);"]
    if mn == "XBA":
        return ["A = (uint16_t)(A >> 8 | A << 8); NZ8(A);"]
    if mn in ("PHA", "PHX", "PHY"):
        return [f"push{w}(({u}){mn[2]});"]
    if mn == "PLA":
        return [f"{{ {u} t = pull{w}(); {set_a('t')} {nz}(t); }}"]
    if mn in ("PLX", "PLY"):
        return [f"{mn[2]} = pull{w}(); {nz}({mn[2]});"]
    if mn == "PHB": return ["push8(DB);"]
    if mn == "PLB": return ["DB = pull8(); NZ8(DB);"]
    if mn == "PHD": return ["push16(D);"]
    if mn == "PLD": return ["D = pull16(); NZ16(D);"]
    if mn == "PHK": return ["push8(cpu.pb);"]
    if mn == "PHP": return ["push8(GETP());"]
    if mn == "PLP": return ["SETP(pull8());"]
    if mn == "PEA": return [f"push16({hx(i.operand)});"]
    if mn == "PEI": return [f"push16(rd16_b0(DP({hx(i.operand, 2)})));"]
    if mn == "PER": return [f"push16({hx(rel_target(i) & 0xFFFF)});"]
    if mn in ("REP", "SEP"):
        v = 0 if mn == "REP" else 1
        out = []
        names = {0x80: "fN", 0x40: "fV", 0x20: "cpu.mf", 0x10: "cpu.xf", 0x08: "cpu.dflag",
                 0x04: "cpu.i", 0x02: "fZ", 0x01: "fC"}
        for bit, name in names.items():
            if i.operand & bit:
                if bit in (0x20, 0x10) and v == 0:
                    out.append(f"if (!cpu.e) {name} = 0;")
                else:
                    out.append(f"{name} = {v};")
        if mn == "SEP" and i.operand & 0x10:
            out.append("X &= 0xff; Y &= 0xff;")
        return out
    simple = {"CLC": "fC = 0;", "SEC": "fC = 1;", "CLI": "cpu.i = 0;", "SEI": "cpu.i = 1;",
              "CLD": "cpu.dflag = 0;", "SED": "cpu.dflag = 1;", "CLV": "fV = 0;", "NOP": "", "WDM": ""}
    if mn in simple:
        return [simple[mn]] if simple[mn] else []
    if mn == "XCE":
        return ["{ uint8_t t = fC; fC = cpu.e; cpu.e = t; if (cpu.e) { cpu.mf = cpu.xf = 1; "
                "X &= 0xff; Y &= 0xff; cpu.s = 0x100 | (cpu.s & 0xff); } }"]
    if mn in ("MVN", "MVP"):
        dst, src = i.operand & 0xFF, i.operand >> 8
        step = "1" if mn == "MVN" else "-1"
        return [f"DB = {hx(dst, 2)}; block_move({hx(dst, 2)}, {hx(src, 2)}, {step}, &A, &X, &Y);"]
    raise ValueError(f"no emitter for {mn} {mode} at {i.addr:06X}")


def width_cases(i):
    """[(condition or None, width)] for an instruction's operation."""
    if i.mnem in M_OPS:
        flag, known = "cpu.mf", i.m_in
    elif i.mnem in X_OPS:
        flag, known = "cpu.xf", i.x_in
    else:
        return [(None, None)]
    if known == W8:
        return [(None, 8)]
    if known == W16:
        return [(None, 16)]
    return [(flag, 8), (None, 16)]


class RegionEmitter:
    def __init__(self, insns, regions, entries, labels_by_addr):
        self.insns, self.regions, self.entries = insns, regions, entries
        self.labels_by_addr = labels_by_addr

    def fn_name(self, region_index):
        return f"R_{self.regions[region_index][0].addr:06x}"

    def target_ref(self, addr):
        """(function name or None) for a call/jump target outside a region."""
        i = self.insns.get(addr)
        if i is not None and addr in self.entries:
            return self.fn_name(i.region)
        return None

    def jump(self, i, target, region_set, backward_ok=True):
        """C that goes to target (an address in i's bank, or long)."""
        if target in region_set:
            if target <= i.addr and backward_ok:
                return f"{{ POLL({hx(target & 0xFFFF)}); goto L_{target:06x}; }}"
            return f"goto L_{target:06x};"
        if (target >> 16) == (i.addr >> 16):
            return f"{{ SAVE(); return PBA({hx(target & 0xFFFF)}); }}"
        return f"{{ SAVE(); return {hx(target, 6)}; }}"

    def emit_call(self, i, target_expr, target_static, ret16, long_call):
        """JSR/JSL: push the return address and run the callee. When it comes
        back the usual way (to the return address, the stack where it was),
        carry on here; anything else goes to finish_call (src/runtime.c)."""
        out = [f"{{ uint16_t s0 = cpu.s; uint32_t ret = PBA({hx(ret16)});"]
        if long_call:
            out.append("  uint8_t pb0 = cpu.pb; push8(pb0);")
        out.append(f"  push16({hx((ret16 - 1) & 0xFFFF)}); SAVE();")
        fn = self.target_ref(target_static) if target_static is not None else None
        if fn:
            if long_call:
                out.append(f"  cpu.pb = {hx(i.operand >> 16, 2)};")
            out.append(f"  uint32_t r = {fn}({target_expr});")
            out.append("  if (r != ret || cpu.s != s0) { if (!finish_call(r, ret, s0)) return g_unwind; }")
            if long_call:
                out.append("  else cpu.pb = pb0;")
        else:
            out.append(f"  if (!call_pc({target_expr}, ret, s0)) return g_unwind;")
        out.append("  RELOAD(); }")
        return out

    def control(self, i, region_set):
        """C for control-flow instructions; None if i isn't one."""
        mn = i.mnem
        nxt16 = i.next & 0xFFFF
        if mn in BRANCHES:
            t = rel_target(i)
            cond = BRANCHES[mn]
            j = self.jump(i, t, region_set)
            return [j] if cond is None else [f"if ({cond}) {j}"]
        if mn == "JMP" and i.mode == "abs":
            return [self.jump(i, (i.addr & 0xFF0000) | i.operand, region_set)]
        if mn == "JML" and i.mode == "absl":
            t = canonical(i.operand)
            if t in region_set:
                return [f"cpu.pb = {hx(i.operand >> 16, 2)}; goto L_{t:06x};"]
            return [f"{{ SAVE(); return {hx(i.operand, 6)}; }}"]
        if mn == "JMP" and i.mode == "absi":
            return [f"{{ SAVE(); return PBA(rd16({hx(i.operand)})); }}"]
        if mn == "JMP" and i.mode == "absix":
            return [f"{{ SAVE(); return PBA(rd16(PBA((uint16_t)({hx(i.operand)} + X)))); }}"]
        if mn == "JML" and i.mode == "absil":
            return [f"{{ SAVE(); return rd24({hx(i.operand)}); }}"]
        if mn == "JSR" and i.mode == "abs":
            t = (i.addr & 0xFF0000) | i.operand
            return self.emit_call(i, f"PBA({hx(i.operand)})", t, nxt16, False)
        if mn == "JSR" and i.mode == "absix":
            return self.emit_call(i, f"PBA(rd16(PBA((uint16_t)({hx(i.operand)} + X))))", None, nxt16, False)
        if mn == "JSL":
            return self.emit_call(i, hx(i.operand, 6), canonical(i.operand), nxt16, True)
        if mn == "RTS":
            return ["{ uint16_t r = pull16(); SAVE(); return PBA((uint16_t)(r + 1)); }"]
        if mn == "RTL":
            return ["{ uint16_t r = pull16(); uint32_t b = pull8(); SAVE(); return b << 16 | (uint16_t)(r + 1); }"]
        if mn == "RTI":
            return ["{ SETP(pull8()); uint16_t r = pull16(); uint32_t b = cpu.e ? cpu.pb : pull8(); "
                    "SAVE(); return b << 16 | r; }"]
        if mn == "WAI":
            return [f"SAVE(); return PC_YIELD | PBA({hx(nxt16)});"]
        if mn in ("STP", "BRK", "COP"):
            return [f"SAVE(); return PC_STOP | PBA({hx(i.addr & 0xFFFF)});"]
        return None

    def assume(self, i):
        m = {W8: 1, W16: 0}.get(i.m_in, 2)
        x = {W8: 1, W16: 0}.get(i.x_in, 2)
        if m == 2 and x == 2:
            return None
        return f"ASSUME_MX({m}, {x}, {hx(i.addr, 6)});"

    def emit_region(self, idx):
        region = self.regions[idx]
        region_set = {i.addr for i in region}
        name = self.fn_name(idx)
        if region[0].addr in NATIVE:
            name += "_recomp"
        targets = set()
        for k, i in enumerate(region):
            t = static_target(i)
            if t is not None and t in region_set:
                targets.add(t)
            if i.next in region_set and (k + 1 == len(region) or region[k + 1].addr != i.next):
                targets.add(i.next)
        entries = [i for i in region if i.addr in self.entries]
        lines = [f"uint32_t {name}(uint32_t entry) {{", "  LOAD();"]
        # Most calls enter at the first instruction: tested before the
        # switch (a search through all the entries).
        first = region[0]
        if first.addr in self.entries:
            lines.append(f"  if ((entry & 0xffff) == {hx(first.addr & 0xFFFF)}) {{ {self.assume(first) or ''} goto L_{first.addr:06x}; }}")
        lines.append("  switch (entry & 0xffff) {")
        for i in entries:
            chk = self.assume(i) or ""
            lines.append(f"    case {hx(i.addr & 0xFFFF)}: {chk} goto L_{i.addr:06x};")
        lines.append("    default: SAVE(); return PC_STOP | entry;")
        lines.append("  }")
        for k, i in enumerate(region):
            labels = self.labels_by_addr.get(i.addr)
            comment = f"  // {i.mnem} {i.mode}" + (f"  [{labels}]" if labels else "")
            if i.addr in targets or i.addr in self.entries:
                lines.append(f"L_{i.addr:06x}:")
            lines.append(comment)
            if i.assumed and i.addr not in self.entries:
                chk = self.assume(i)
                if chk:
                    lines.append("  " + chk)
            cyc = CYCLES[i.op]
            ctl = self.control(i, region_set)
            body = []
            if ctl is not None:
                body = ctl
            else:
                cases = width_cases(i)
                if len(cases) == 1:
                    w = cases[0][1]
                    body = emit_op(i, w)
                    consts = const_cases(i)
                    if consts:
                        CONSTS.update(consts)
                        fast = emit_op(i, w)
                        CONSTS.clear()
                        cond = " && ".join(f"{r} == {hx(k, 4 if r == 'D' else 2)}" for r, k in consts.items())
                        body = [f"if ({cond}) {{"] + ["  " + b for b in fast] + \
                               ["} else {"] + ["  " + b for b in body] + ["}"]
                    if w == 16:
                        cyc += 1
                else:
                    body = [f"if ({cases[0][0]}) {{"] + ["  " + b for b in emit_op(i, 8)] + \
                           ["} else {"] + ["  " + b for b in emit_op(i, 16)] + ["}"]
            lines.append(f"  CYC({cyc});")
            lines += ["  " + b for b in body]
            falls = i.mnem not in NO_FALLTHROUGH and i.mnem != "WAI" and i.mnem not in ("STP", "BRK", "COP")
            emitted_next = region[k + 1].addr if k + 1 < len(region) else None
            if falls and emitted_next != i.next:
                # The next instruction isn't emitted next: it's elsewhere in
                # this function (a goto), or in another one.
                if i.next in region_set:
                    lines.append(f"  goto L_{i.next:06x};")
                else:
                    lines.append(f"  SAVE(); return PBA({hx(i.next & 0xFFFF)});")
        lines.append("}")
        return lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument("--rom", required=True)
    ap.add_argument("--asar", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--trace", action="append", default=[])
    ap.add_argument("--probe-cache")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    rom = open(args.rom, "rb").read()
    pr = probe(args.root, args.asar, args.rom, args.probe_cache)
    tflags, tedges = load_traces(args.trace)
    insns = decode(pr["code"], rom, tflags)
    fixed = resync(insns, rom, tflags)
    if fixed:
        print(f"re-decoded {len(fixed)} instructions the disassembly has at the wrong width: "
              + " ".join(f"{a:06X}" for a in fixed[:20]), file=sys.stderr)

    dyn = collections.defaultdict(set)
    dyn_targets = set()
    for frm, to in tedges:
        dyn_targets.add(to)
        if frm in insns and insns[frm].mode in ("absi", "absix", "absil"):
            dyn[frm].add(to)
    missing = sorted(t for t in dyn_targets if t not in insns)
    if missing:
        print(f"warning: {len(missing)} traced targets aren't instructions in the disassembly: "
              + " ".join(f"{t:06X}" for t in missing[:20]), file=sys.stderr)

    infer_widths(insns, tflags, dyn)
    for _ in range(8):
        conflicts = width_conflicts(insns, tflags)
        if not conflicts:
            break
        redone = []
        for a, m, x in conflicts:
            if a in insns:
                redone += redecode(insns, rom, a, m, x)
        print("re-decoded at the widths the analysis gives (the disassembly has other ones, no trace "
              "ran them): " + " ".join(f"{a:06X}" for a in redone[:20]), file=sys.stderr)
        for i in insns.values():
            i.m_in = i.x_in = i.m_pred = i.x_pred = 0
            i.assumed = False
        infer_widths(insns, tflags, dyn)
    regions = make_regions(insns)

    labels_by_addr = collections.defaultdict(list)
    for name, a in pr["labels"].items():
        if a in insns:
            labels_by_addr[a].append(name)
    labels_by_addr = {a: " ".join(sorted(v)) for a, v in labels_by_addr.items()}

    entries = set()
    for r in regions:
        entries.add(r[0].addr)
    entries |= set(a for a in labels_by_addr)
    for i in insns.values():
        if i.mnem in ("JSR", "JSL", "WAI") and i.next in insns:
            entries.add(i.next)
        t = static_target(i)
        if t is not None and t in insns and (insns[t].region != i.region or t <= i.addr):
            # Other regions jump there; or a backward branch, whose POLL can
            # return there for an interrupt.
            entries.add(t)
    entries |= set(t for t in dyn_targets if t in insns)

    em = RegionEmitter(insns, regions, entries, labels_by_addr)
    by_file = collections.defaultdict(list)
    for idx, r in enumerate(regions):
        by_file[r[0].file].append(idx)
    with open(os.path.join(args.out, "regions.h"), "w") as h:
        h.write("// Generated by tools/recomp/recomp.py from the player's ROM. Don't edit.\n#pragma once\n#include <stdint.h>\n")
        for idx in range(len(regions)):
            h.write(f"uint32_t {em.fn_name(idx)}(uint32_t entry);\n")
            if regions[idx][0].addr in NATIVE:
                h.write(f"uint32_t {em.fn_name(idx)}_recomp(uint32_t entry);\n")
    sources = []
    for f, idxs in sorted(by_file.items()):
        path = os.path.join(args.out, f.replace(".asm", ".c"))
        sources.append(os.path.basename(path))
        with open(path, "w") as out:
            out.write(f"// Generated by tools/recomp/recomp.py from {f} and the player's ROM. Don't edit.\n")
            out.write('#include "cpu65.h"\n#include "regions.h"\n\n')
            for idx in idxs:
                out.write("\n".join(em.emit_region(idx)) + "\n\n")
    with open(os.path.join(args.out, "entries.c"), "w") as out:
        out.write("// Generated by tools/recomp/recomp.py. Don't edit.\n")
        out.write('#include "cpu65.h"\n#include "regions.h"\n\n')
        es = sorted(entries)
        out.write(f"const uint32_t g_num_entries = {len(es)};\n")
        out.write("const uint32_t g_entry_pcs[] = {\n")
        for k in range(0, len(es), 8):
            out.write("  " + ", ".join(hx(e, 6) for e in es[k:k + 8]) + ",\n")
        out.write("};\nconst RegionFn g_entry_fns[] = {\n")
        for k in range(0, len(es), 4):
            out.write("  " + ", ".join(em.fn_name(insns[e].region) for e in es[k:k + 4]) + ",\n")
        out.write("};\n")
        # A page index: for each code bank, where each 256-byte page's
        # entries start in the sorted table, so a lookup searches only the
        # few entries of one page.
        banks = sorted(set(e >> 16 for e in es))
        bank_slot = [0xFF] * 256
        for k, b in enumerate(banks):
            bank_slot[b] = k
        out.write("const uint8_t g_entry_bank_slot[256] = {\n")
        for k in range(0, 256, 16):
            out.write("  " + ", ".join(str(v) for v in bank_slot[k:k + 16]) + ",\n")
        out.write("};\n")
        out.write(f"const uint16_t g_entry_page_start[{len(banks)}][257] = {{\n")
        import bisect
        for b in banks:
            starts = [bisect.bisect_left(es, (b << 16) | (pg << 8)) for pg in range(257)]
            out.write("  {" + ", ".join(str(v) for v in starts) + "},\n")
        out.write("};\n")
        if len(es) > 0xFFFF:
            raise SystemExit("too many entries for the page index")
    with open(os.path.join(args.out, "sources.txt"), "w") as out:
        out.write("\n".join(sources + ["entries.c"]) + "\n")

    unknown_m = sum(1 for i in insns.values() if i.mnem in M_OPS and i.m_in == WANY)
    unknown_x = sum(1 for i in insns.values() if i.mnem in X_OPS and i.x_in == WANY)
    traced = sum(1 for a in insns if a in tflags)
    print(f"{len(insns)} instructions ({traced} traced), {len(regions)} regions, {len(entries)} entries; "
          f"width tested at run time: {unknown_m} M, {unknown_x} X")


if __name__ == "__main__":
    main()
