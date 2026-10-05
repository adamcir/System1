.section .text
.code32
.global _start_floppy_i386
.type _start_floppy_i386, @function
.extern kmain_floppy_i386

_start_floppy_i386:
    cli
    mov $stack_top_floppy, %esp

    /* stage2 contract: eax=magic, ebx=boot_info_ptr */
    push %ebx
    push %eax
    call kmain_floppy_i386

1:
    hlt
    jmp 1b

.section .bss
.align 16
stack_floppy:
    .skip 16384
stack_top_floppy:
