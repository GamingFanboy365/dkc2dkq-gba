@ The cartridge's start: the header (tools/make_cartridge.py fills it in),
@ the SNES ROM's offset in the cartridge (also filled in by it), then
@ stacks, IWRAM's data copied from ROM, BSS cleared, and main.
    .section .crt0, "ax"
    .arm
    .global _start
_start:
    b       start
    .fill   0xbc, 1, 0          @ header: logo, title, codes, checksum

    .global g_snes_rom_offset
g_snes_rom_offset:              @ 0x080000C0
    .word   0

start:
    mov     r0, #0x12           @ IRQ mode
    msr     cpsr_c, r0
    ldr     sp, =0x03007fa0
    mov     r0, #0x1f           @ system mode
    msr     cpsr_c, r0
    ldr     sp, =0x03007f00

    ldr     r0, =__iwram_lma    @ IWRAM code and data from ROM
    ldr     r1, =__iwram_start
    ldr     r2, =__iwram_end
1:  cmp     r1, r2
    ldrlt   r3, [r0], #4
    strlt   r3, [r1], #4
    blt     1b

    mov     r3, #0
    ldr     r1, =__bss_start
    ldr     r2, =__bss_end
2:  cmp     r1, r2
    strlt   r3, [r1], #4
    blt     2b
    ldr     r1, =__ewram_bss_start
    ldr     r2, =__ewram_bss_end
3:  cmp     r1, r2
    strlt   r3, [r1], #4
    blt     3b

    ldr     r0, =irq_entry          @ the BIOS calls [0x03007FFC] on an interrupt
    ldr     r1, =0x03007ffc
    str     r0, [r1]

    ldr     r0, =main
    mov     lr, pc
    bx      r0
4:  b       4b

@ Interrupts: the BIOS has saved r0-r3, r12 and lr and is in IRQ mode, in
@ ARM; irq_handler (C, Thumb) does the work, given the interrupted PC + 4
@ (IRQ mode's lr, which the BIOS saved last).
    .global irq_entry
irq_entry:
    stmfd   sp!, {lr}
    ldr     r0, [sp, #24]
    ldr     r1, =irq_handler
    mov     lr, pc
    bx      r1
    ldmfd   sp!, {lr}
    bx      lr
    .pool
