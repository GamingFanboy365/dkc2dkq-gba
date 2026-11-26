#!/usr/bin/env python3
"""Builds the GBA cartridge: the engine, then the player's SNES ROM.

  make_cartridge.py ENGINE.gba DKC2.sfc OUT.gba [--logo-from ANY.gba]

The SNES ROM is checked (Donkey Kong Country 2, USA, Rev 1), appended at the
next 64 KB boundary, and its offset written where crt0.S expects it (0xC0).
The header gets a title, codes and its checksum. Real hardware also wants
the boot logo in the header; it isn't in this repository: --logo-from copies
it from any GBA cartridge dump you have. Emulators run the cartridge
without it.
"""
import argparse
import hashlib
import sys

ROM_SHA256 = "b79c2bb86f6fc76e1fc61c62fc16d51c664c381e58bc2933be643bbc4d8b610c"
OFFSET_FIELD = 0xC0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("engine")
    ap.add_argument("snes_rom")
    ap.add_argument("out")
    ap.add_argument("--logo-from")
    args = ap.parse_args()

    engine = bytearray(open(args.engine, "rb").read())
    rom = open(args.snes_rom, "rb").read()
    if len(rom) % 1024 == 512:
        rom = rom[512:]  # a copier header
    if hashlib.sha256(rom).hexdigest() != ROM_SHA256:
        sys.exit(f"{args.snes_rom} isn't Donkey Kong Country 2 (USA, Rev 1)")

    if len(engine) < 0xC4 or engine[OFFSET_FIELD:OFFSET_FIELD + 4] != b"\0\0\0\0":
        sys.exit(f"{args.engine} doesn't look like the port's engine")
    offset = (len(engine) + 0xFFFF) & ~0xFFFF
    engine[OFFSET_FIELD:OFFSET_FIELD + 4] = offset.to_bytes(4, "little")

    if args.logo_from:
        engine[0x04:0xA0] = open(args.logo_from, "rb").read()[0x04:0xA0]
    engine[0xA0:0xAC] = b"DKC2 PORT\0\0\0"
    engine[0xAC:0xB0] = b"ZDKE"
    engine[0xB0:0xB2] = b"00"
    engine[0xB2] = 0x96
    engine[0xB3:0xBD] = bytes(10)
    engine[0xBD] = (-(sum(engine[0xA0:0xBD]) + 0x19)) & 0xFF

    out = engine + bytes(offset - len(engine)) + rom
    open(args.out, "wb").write(out)
    print(f"{args.out}: engine {len(engine)} bytes, SNES ROM at 0x{offset:X}, {len(out)} bytes in all")


if __name__ == "__main__":
    main()
