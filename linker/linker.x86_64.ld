ENTRY(_start_x86_64)

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
