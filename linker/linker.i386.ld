ENTRY(_start_i386)

SECTIONS {
  . = 1M;

  .multiboot2 : {
    *(.multiboot2)
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
}
