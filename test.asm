section .text
global _start
_start:
    int 0x80 ; Trigger syscall (implement a write syscall in your kernel)
.hang:
    hlt
    jmp .hang
