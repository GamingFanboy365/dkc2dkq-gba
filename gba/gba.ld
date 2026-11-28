/* The GBA's memory: the engine's code and constants in ROM, then the
   player's SNES ROM appended by tools/make_cartridge.py. IWRAM (32 KB, fast)
   holds data and hot code; EWRAM (256 KB) the SNES's WRAM, VRAM and the
   rest of the hardware model. */
ENTRY(_start)

MEMORY {
  rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
  iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 0x7400   /* the rest (2.8 KB): stacks */
  ewram (rw)  : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS {
  .text : {
    KEEP(*(.crt0))
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > rom

  .iwram : {
    __iwram_start = .;
    *(.iwram .iwram.*)
    *(.data .data.*)
    . = ALIGN(4);
    __iwram_end = .;
  } > iwram AT > rom
  __iwram_lma = LOADADDR(.iwram);

  .bss (NOLOAD) : {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > iwram

  .ewram_bss (NOLOAD) : {
    __ewram_bss_start = .;
    *(.ewram_bss .ewram_bss.*)
    . = ALIGN(4);
    __ewram_bss_end = .;
  } > ewram

  __rom_end = LOADADDR(.iwram) + SIZEOF(.iwram);

  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.comment) }
}
