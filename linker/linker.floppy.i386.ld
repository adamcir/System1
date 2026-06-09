ENTRY(_start_floppy_i386)

SECTIONS {
  . = 0x00010000;
  __kernel_start = .;

  .text : {
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
  }

  .data : {
    *(.data*)
  }

  .bss : {
    *(.bss*)
    *(COMMON)
  }
  . = ALIGN(4096);
  __kernel_end = .;
}
