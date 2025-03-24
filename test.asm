section .text
global _start
_start:
    jmp hang
.hang:
    hlt
    jmp .hang
