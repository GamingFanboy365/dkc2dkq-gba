"""Asks asar where every 65816 instruction and label of the disassembly is.

The disassembly's `;$XXXXXX` comments are hand-kept and miss some lines, so
this assembles a scratch copy with a `print` before each instruction line and
after each label, and reads the addresses back. Only the code that asar
actually assembles (the right side of `if !version` blocks) prints.

Result (cached as JSON):
  code:   [[addr, file, line, mnemonic, marker], ...] in source order;
          marker is the disassembly's routine column: '\\' start, '/' end,
          '>' a one-line routine, '|' inside, '' unknown
  labels: {name: addr}; sublabels as parent.sub
"""
import json
import os
import re
import shutil
import subprocess
import tempfile

MNEMONICS = set("""ADC AND ASL BCC BCS BEQ BIT BMI BNE BPL BRA BRK BRL BVC BVS CLC CLD CLI CLV
CMP COP CPX CPY DEC DEX DEY EOR INC INX INY JML JMP JSL JSR LDA LDX LDY LSR MVN MVP NOP ORA PEA
PEI PER PHA PHB PHD PHK PHP PHX PHY PLA PLB PLD PLP PLX PLY REP ROL ROR RTI RTL RTS SBC SEC SED
SEI SEP STA STP STX STY STZ TAX TAY TCD TCS TDC TRB TSB TSC TSX TXA TXS TXY TYA TYX WAI WDM XBA
XCE""".split())
WORD_RE = re.compile(r"^\s*([A-Za-z]{3})(\.[bwlBWL])?(\s|;|$)")
MACRO_INSN_RE = re.compile(r"^\s*%(lda_sound|pea_[a-z_]+|return)\b")
GLOBAL_LABEL_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*):(.*)$")
SUB_LABEL_RE = re.compile(r"^\s*(\.+)([A-Za-z0-9_]+):?(\s|;|$)")
MARKER_RE = re.compile(r";\$[0-9A-Fa-f]{6}\s+([\\/|>])")


def _instruction(text):
    """The mnemonic if this text is a 65816 instruction, else None."""
    m = WORD_RE.match(text)
    if m and m.group(1) == m.group(1).upper() and m.group(1) in MNEMONICS:
        return m.group(1)
    m = MACRO_INSN_RE.match(text)
    return "%" + m.group(1) if m else None


def _classify(lines):
    """Yields (index, kind, data) for 65816 lines: kind 'label' (data = full
    name) or 'insn' (data = (mnemonic, marker, text))."""
    arch65, based = True, False
    parents = [None]
    for i, line in enumerate(lines):
        low = line.strip().lower()
        if low.startswith("arch "):
            arch65 = low.split()[1] == "65816"
        elif low.startswith("base "):
            based = low.split()[1] != "off"
        if not arch65 or based:
            continue
        text = line
        g = GLOBAL_LABEL_RE.match(line)
        if g:
            parents = [g.group(1)]
            yield i, "label", g.group(1)
            text = g.group(2)
        else:
            sl = SUB_LABEL_RE.match(line)
            if sl:
                depth = len(sl.group(1))
                parents = parents[:depth] + [sl.group(2)]
                yield i, "label", ".".join(p or "" for p in parents)
                continue
        mnem = _instruction(text)
        if mnem:
            mk = MARKER_RE.search(text)
            yield i, "insn", (mnem, mk.group(1) if mk else "", text)


def probe(root, asar, baserom, cache=None):
    if cache and os.path.exists(cache):
        return json.load(open(cache))
    banks = sorted(f for f in os.listdir(root) if re.match(r"bank_[0-9A-F]{2}\.asm$", f))
    insns, label_names = {}, {}
    with tempfile.TemporaryDirectory() as tmp:
        for f in os.listdir(root):
            if f.endswith(".asm"):
                shutil.copy(os.path.join(root, f), tmp)
        shutil.copy(baserom, os.path.join(tmp, "baserom.sfc"))
        for b in banks:
            lines = open(os.path.join(root, b), encoding="utf-8").read().split("\n")
            before, after = {}, {}
            insns[b], label_names[b] = [], []
            for i, kind, data in _classify(lines):
                if kind == "label":
                    # After the label line; a label sharing its line with an
                    # instruction is printed before that instruction's print.
                    after.setdefault(i, []).append(f'print "L {b} {len(label_names[b])} ",hex(realbase())')
                    label_names[b].append(data)
                else:
                    before.setdefault(i, []).append(f'print "C {b} {len(insns[b])} ",hex(realbase())')
                    insns[b].append((i + 1,) + data[:2])
            out = []
            for i, line in enumerate(lines):
                g = GLOBAL_LABEL_RE.match(line)
                if g and i in before:
                    out.append(f"{g.group(1)}:")
                    out += after.pop(i, [])
                    out += before[i]
                    out.append(g.group(2))
                    continue
                out += before.get(i, [])
                out.append(line)
                out += after.get(i, [])
            open(os.path.join(tmp, b), "w", encoding="utf-8").write("\n".join(out))
        res = subprocess.run([asar, "all.asm", "probe.sfc"], cwd=tmp, capture_output=True, text=True)
        if res.returncode:
            raise SystemExit(res.stdout[-3000:] + res.stderr[-3000:])

    code, labels = [], {}
    for line in res.stdout.splitlines():
        p = line.split()
        if len(p) == 4 and p[0] == "C":
            ln, mnem, marker = insns[p[1]][int(p[2])]
            code.append([int(p[3], 16), p[1], ln, mnem, marker])
        elif len(p) == 4 and p[0] == "L":
            labels.setdefault(label_names[p[1]][int(p[2])], int(p[3], 16))
    result = {"code": code, "labels": labels}
    if cache:
        json.dump(result, open(cache, "w"))
    return result


if __name__ == "__main__":
    import sys
    r = probe(sys.argv[1], sys.argv[2], sys.argv[3])
    print(len(r["code"]), "instructions,", len(r["labels"]), "labels")
