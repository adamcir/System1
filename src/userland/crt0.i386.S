.section .text
.code32
.global _start
.extern main
.extern _exit

_start:
    movl (%esp), %eax
    leal 4(%esp), %ebx
    leal 4(%ebx,%eax,4), %ecx

    pushl %ecx
    pushl %ebx
    pushl %eax
    call main
    addl $12, %esp

    pushl %eax
    call _exit
1:
    hlt
    jmp 1b
