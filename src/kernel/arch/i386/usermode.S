.section .text
.code32

.extern syscall_dispatch
.extern g_usermode_saved_esp
.extern g_usermode_saved_eflags

.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    cld
    pusha

    /*
     * int 0x80 enters through an interrupt gate, so IF is cleared by the CPU.
     * Re-enable IRQs after the register frame is safe. This is required for
     * blocking TTY read(): tty_readline() sleeps with HLT and must be woken by
     * the keyboard IRQ.
     */
    sti

    movl 28(%esp), %eax
    movl 16(%esp), %ebx
    movl 24(%esp), %ecx
    movl 20(%esp), %edx
    movl 4(%esp), %esi

    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    call syscall_dispatch
    addl $20, %esp

    cli
    movl %eax, 28(%esp)
    popa
    iret

.global usermode_i386_load_gdt
.type usermode_i386_load_gdt, @function
usermode_i386_load_gdt:
    movl 4(%esp), %eax
    lgdt (%eax)
    ljmp $0x08, $1f
1:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret

.global usermode_i386_load_tss
.type usermode_i386_load_tss, @function
usermode_i386_load_tss:
    movw 4(%esp), %ax
    ltr %ax
    ret

.global usermode_i386_enter
.type usermode_i386_enter, @function
usermode_i386_enter:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl %esp, g_usermode_saved_esp
    pushfl
    popl %ecx
    movl %ecx, g_usermode_saved_eflags

    movl 20(%esp), %eax
    movl 24(%esp), %edx

    movw $0x23, %cx
    movw %cx, %ds
    movw %cx, %es
    movw %cx, %fs
    movw %cx, %gs

    pushl $0x23
    pushl %edx
    pushl g_usermode_saved_eflags
    orl $0x200, (%esp)
    pushl $0x1B
    pushl %eax
    iret

.global usermode_i386_return
.type usermode_i386_return, @function
usermode_i386_return:
    movl 4(%esp), %eax
    movl g_usermode_saved_esp, %esp

    movw $0x10, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    pushl g_usermode_saved_eflags
    popfl
    ret
