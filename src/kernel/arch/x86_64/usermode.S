.section .text
.code64
.extern syscall_dispatch
.extern g_usermode_saved_rsp
.extern g_usermode_saved_rflags

/* Compatibility int 0x80: eax syscall, ebx/ecx/edx/esi arguments. */
.global syscall_entry
.type syscall_entry,@function
syscall_entry:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    sti
    movl 112(%rsp), %edi
    movl 104(%rsp), %esi
    movl 96(%rsp), %edx
    movl 88(%rsp), %ecx
    movl 80(%rsp), %r8d
    call syscall_dispatch
    cli
    movl %eax, 112(%rsp)
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq

.global usermode_x64_enter
.type usermode_x64_enter,@function
usermode_x64_enter:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, g_usermode_saved_rsp(%rip)
    pushfq
    popq g_usermode_saved_rflags(%rip)
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushq $0x23
    pushq %rsi
    pushfq
    orq $0x200,(%rsp)
    pushq $0x1b
    pushq %rdi
    iretq

.global usermode_x64_return
.type usermode_x64_return,@function
usermode_x64_return:
    movl %edi,%eax
    movq g_usermode_saved_rsp(%rip),%rsp
    movw $0x10,%dx
    movw %dx,%ds
    movw %dx,%es
    movw %dx,%fs
    movw %dx,%gs
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    pushq g_usermode_saved_rflags(%rip)
    popfq
    ret
.section .note.GNU-stack,"",@progbits
