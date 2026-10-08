/* Native SMOD v1 image is position independent, with entry offset 0.
 * The output ELF is a *build-only intermediate*: .mod contains raw bytes.
 */
ENTRY(smod_entry)
SECTIONS {
  . = 0;
  .text : {
    KEEP(*(.text.smod_entry))
    *(.text*)
    *(.rodata*)
  }
  .data : { *(.data*) }
  .got : { *(.got*) }
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
