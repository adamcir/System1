OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x00400000;
    __sprg_base = .;

    .sprg_header : {
        BYTE(0x53) BYTE(0x50) BYTE(0x52) BYTE(0x47) /* SPRG */
        LONG(1)                  /* version */
        LONG(2)                  /* arch: x86_64 */
        LONG(_start)             /* entry */
        LONG(__sprg_phdr - __sprg_base)
        LONG(1)                  /* phnum */
        LONG(0)                  /* flags */

        __sprg_phdr = .;
        LONG(1)                  /* type: LOAD */
        LONG(__program_start - __sprg_base)
        LONG(__program_start)
        LONG(__program_filesz)
        LONG(__program_memsz)
        LONG(0x5)                /* R | X */
        LONG(0x1000)
        LONG(0)
    }

    __program_start = .;

    .text : {
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    __program_end = .;

    __program_filesz = __bss_start - __program_start;
    __program_memsz = __program_end - __program_start;
}
