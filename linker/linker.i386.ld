ENTRY(_start_i386)

SECTIONS {
  . = 1M;
  __kernel_start = .;

  .multiboot2 ALIGN(8) : {
    KEEP(*(.multiboot2))
  }

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
