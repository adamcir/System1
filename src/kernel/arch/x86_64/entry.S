.section .text
.code32
.global _start_x86_64
.type _start_x86_64, @function
.extern kmain_x86_64

_start_x86_64:
    cli

    mov %eax, mb2_magic_saved
    mov %ebx, mb2_info_saved

    /* Build minimal identity mapping: first 2 MiB */
    movl $pdpt_table, %eax
    orl  $0x3, %eax
    movl %eax, pml4_table

    movl $pd_table, %eax
    orl  $0x3, %eax
    movl %eax, pdpt_table

    /* 2 MiB page: present + rw + huge */
    movl $0x00000083, pd_table

    /* Load 64-bit GDT */
    lgdt gdt64_ptr

    /* Enable PAE */
    mov %cr4, %eax
    or  $0x20, %eax
    mov %eax, %cr4

    /* Set EFER.LME */
    mov $0xC0000080, %ecx
    rdmsr
    or  $0x100, %eax
    wrmsr

    /* Load CR3 with PML4 */
    mov $pml4_table, %eax
    mov %eax, %cr3

    /* Enable paging + keep protected mode */
    mov %cr0, %eax
    or  $0x80000001, %eax
    mov %eax, %cr0

    ljmp $0x08, $long_mode_entry

.code64
long_mode_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    mov $stack_top_x64, %rsp

    mov mb2_magic_saved(%rip), %edi
    mov mb2_info_saved(%rip), %esi
    call kmain_x86_64

1:
    hlt
    jmp 1b

.section .data
.align 8
gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF

gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

mb2_magic_saved:
    .long 0
mb2_info_saved:
    .long 0

.section .bss
.align 4096
pml4_table:
    .skip 4096
pdpt_table:
    .skip 4096
pd_table:
    .skip 4096

.align 16
stack_x64:
    .skip 16384
stack_top_x64:
