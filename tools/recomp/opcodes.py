"""The 65816 instruction set: mnemonic and addressing mode per opcode.

Modes: imp (implied), acc (accumulator), immm (immediate, sized by M),
immx (sized by X), imm8, imm16 (PEA), dp, dpx, dpy, dpi (dp), dpix (dp,x),
dpiy (dp),y, dpil [dp], dpily [dp],y, abs, absx, absy, absl, abslx, sr,
sriy (sr,s),y, rel, rell (16-bit relative), absi (abs), absix (abs,x),
absil [abs], bm (block move).
"""

_TABLE = """
BRK imm8|ORA dpix|COP imm8|ORA sr|TSB dp|ORA dp|ASL dp|ORA dpil|PHP imp|ORA immm|ASL acc|PHD imp|TSB abs|ORA abs|ASL abs|ORA absl
BPL rel|ORA dpiy|ORA dpi|ORA sriy|TRB dp|ORA dpx|ASL dpx|ORA dpily|CLC imp|ORA absy|INC acc|TCS imp|TRB abs|ORA absx|ASL absx|ORA abslx
JSR abs|AND dpix|JSL absl|AND sr|BIT dp|AND dp|ROL dp|AND dpil|PLP imp|AND immm|ROL acc|PLD imp|BIT abs|AND abs|ROL abs|AND absl
BMI rel|AND dpiy|AND dpi|AND sriy|BIT dpx|AND dpx|ROL dpx|AND dpily|SEC imp|AND absy|DEC acc|TSC imp|BIT absx|AND absx|ROL absx|AND abslx
RTI imp|EOR dpix|WDM imm8|EOR sr|MVP bm|EOR dp|LSR dp|EOR dpil|PHA imp|EOR immm|LSR acc|PHK imp|JMP abs|EOR abs|LSR abs|EOR absl
BVC rel|EOR dpiy|EOR dpi|EOR sriy|MVN bm|EOR dpx|LSR dpx|EOR dpily|CLI imp|EOR absy|PHY imp|TCD imp|JML absl|EOR absx|LSR absx|EOR abslx
RTS imp|ADC dpix|PER rell|ADC sr|STZ dp|ADC dp|ROR dp|ADC dpil|PLA imp|ADC immm|ROR acc|RTL imp|JMP absi|ADC abs|ROR abs|ADC absl
BVS rel|ADC dpiy|ADC dpi|ADC sriy|STZ dpx|ADC dpx|ROR dpx|ADC dpily|SEI imp|ADC absy|PLY imp|TDC imp|JMP absix|ADC absx|ROR absx|ADC abslx
BRA rel|STA dpix|BRL rell|STA sr|STY dp|STA dp|STX dp|STA dpil|DEY imp|BIT immm|TXA imp|PHB imp|STY abs|STA abs|STX abs|STA absl
BCC rel|STA dpiy|STA dpi|STA sriy|STY dpx|STA dpx|STX dpy|STA dpily|TYA imp|STA absy|TXS imp|TXY imp|STZ abs|STA absx|STZ absx|STA abslx
LDY immx|LDA dpix|LDX immx|LDA sr|LDY dp|LDA dp|LDX dp|LDA dpil|TAY imp|LDA immm|TAX imp|PLB imp|LDY abs|LDA abs|LDX abs|LDA absl
BCS rel|LDA dpiy|LDA dpi|LDA sriy|LDY dpx|LDA dpx|LDX dpy|LDA dpily|CLV imp|LDA absy|TSX imp|TYX imp|LDY absx|LDA absx|LDX absy|LDA abslx
CPY immx|CMP dpix|REP imm8|CMP sr|CPY dp|CMP dp|DEC dp|CMP dpil|INY imp|CMP immm|DEX imp|WAI imp|CPY abs|CMP abs|DEC abs|CMP absl
BNE rel|CMP dpiy|CMP dpi|CMP sriy|PEI dp|CMP dpx|DEC dpx|CMP dpily|CLD imp|CMP absy|PHX imp|STP imp|JML absil|CMP absx|DEC absx|CMP abslx
CPX immx|SBC dpix|SEP imm8|SBC sr|CPX dp|SBC dp|INC dp|SBC dpil|INX imp|SBC immm|NOP imp|XBA imp|CPX abs|SBC abs|INC abs|SBC absl
BEQ rel|SBC dpiy|SBC dpi|SBC sriy|PEA imm16|SBC dpx|INC dpx|SBC dpily|SED imp|SBC absy|PLX imp|XCE imp|JSR absix|SBC absx|INC absx|SBC abslx
"""

OPCODES = []
for row in _TABLE.strip().split("\n"):
    for cell in row.split("|"):
        mnem, mode = cell.split()
        OPCODES.append((mnem, mode))
assert len(OPCODES) == 256

# Operand bytes per mode (immm/immx: for 8-bit; one more when 16-bit).
OPERAND_SIZE = {
    "imp": 0, "acc": 0, "immm": 1, "immx": 1, "imm8": 1, "imm16": 2, "dp": 1, "dpx": 1,
    "dpy": 1, "dpi": 1, "dpix": 1, "dpiy": 1, "dpil": 1, "dpily": 1, "abs": 2, "absx": 2,
    "absy": 2, "absl": 3, "abslx": 3, "sr": 1, "sriy": 1, "rel": 1, "rell": 2, "absi": 2,
    "absix": 2, "absil": 2, "bm": 2,
}

# Which register width an instruction's operation uses: 'm', 'x' or None.
M_OPS = {"ORA", "AND", "EOR", "ADC", "SBC", "CMP", "BIT", "LDA", "STA", "STZ", "INC", "DEC",
         "ASL", "LSR", "ROL", "ROR", "TSB", "TRB", "PHA", "PLA", "TXA", "TYA"}
X_OPS = {"LDX", "LDY", "STX", "STY", "CPX", "CPY", "INX", "INY", "DEX", "DEY", "PHX", "PHY",
         "PLX", "PLY", "TAX", "TAY", "TSX", "TXY", "TYX"}

# Base CPU cycles per opcode (8-bit registers, no page crossing), for the
# rough clock the host build keeps; one more per 16-bit M/X operation.
_CYCLES = """
7 6 7 4 5 3 5 6 3 2 2 4 6 4 6 5
2 5 5 7 5 4 6 6 2 4 2 2 6 4 7 5
6 6 8 4 3 3 5 6 4 2 2 5 4 4 6 5
2 5 5 7 4 4 6 6 2 4 2 2 4 4 7 5
6 6 2 4 7 3 5 6 3 2 2 3 3 4 6 5
2 5 5 7 7 4 6 6 2 4 3 2 4 4 7 5
6 6 6 4 3 3 5 6 4 2 2 6 5 4 6 5
2 5 5 7 4 4 6 6 2 4 4 2 6 4 7 5
3 6 4 4 3 3 3 6 2 2 2 3 4 4 4 5
2 6 5 7 4 4 4 6 2 5 2 2 4 5 5 5
2 6 2 4 3 3 3 6 2 2 2 4 4 4 4 5
2 5 5 7 4 4 4 6 2 4 2 2 4 4 4 5
2 6 3 4 3 3 5 6 2 2 2 3 4 4 6 5
2 5 5 7 6 4 6 6 2 4 3 3 6 4 7 5
2 6 3 4 3 3 5 6 2 2 2 3 4 4 6 5
2 5 5 7 5 4 6 6 2 4 4 2 8 4 7 5
"""
CYCLES = [int(c) for c in _CYCLES.split()]
assert len(CYCLES) == 256

# Mnemonics the disassembly may spell differently from the table.
ALIASES = {"JML": {"JMP", "JML"}, "JMP": {"JMP", "JML"}, "JSL": {"JSL", "JSR"}, "JSR": {"JSR", "JSL"}}


def same_mnemonic(source, table):
    if source.startswith("%"):
        return True  # a macro; checked by the bytes alone
    return source == table or table in ALIASES.get(source, ())
