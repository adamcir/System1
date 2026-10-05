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

  ASSERT(__kernel_end <= 0x00060000,
         "floppy kernel overlaps reserved i386 userspace at 0x60000")
}
