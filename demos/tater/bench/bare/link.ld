/* Bare image for the C3 ROM loader. Segments stay below 0x3FCCAE00 (the ROM's own area while it loads). */
ENTRY(_start)
MEMORY {
  iram (RWX) : ORIGIN = 0x40380000, LENGTH = 0x10000
  dram (RW)  : ORIGIN = 0x3FC90000, LENGTH = 0x3AE00   /* to 0x3FCCAE00 */
}
SECTIONS {
  .text : { KEEP(*(.text.start)) *(.text .text.*) } > iram
  .rodata : { *(.rodata .rodata.* .srodata .srodata.*) } > dram
  .data : { *(.data .data.* .sdata .sdata.*) } > dram
  .bss (NOLOAD) : { _bss_start = .; *(.bss .bss.* .sbss .sbss.* COMMON) . = ALIGN(4); _bss_end = .; } > dram
  _stack_top = ORIGIN(dram) + LENGTH(dram) - 16;
  /DISCARD/ : { *(.comment) *(.riscv.attributes) *(.eh_frame*) }
}
