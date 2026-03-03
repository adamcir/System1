.section .text
.code32
.global _start_i386
.type _start_i386, @function
.extern kmain_i386

_start_i386:
    cli
    mov $stack_top_i386, %esp

    /* multiboot: eax=magic, ebx=info */
    push %ebx
    push %eax
    call kmain_i386

1:
    hlt
    jmp 1b

.section .bss
.align 16
stack_i386:
    .skip 16384
stack_top_i386:
