; constants for multiboot header
MEMINFO     equ  1<<0    ; 0x1
BOOTDEVICE  equ  1<<1    ; 0x2
VIDEO_MODE  equ  1<<2    ; 0x4 
FLAGS       equ  MEMINFO | BOOTDEVICE | VIDEO_MODE

MAGIC_HEADER       equ  0x1BADB002
CHECKSUM    equ -(MAGIC_HEADER + FLAGS)

BOOTLOADER_MAGIC  equ  0x2BADB002


; set multiboot section
section .multiboot
    align 4
    dd MAGIC_HEADER
    dd FLAGS
    dd CHECKSUM
    ; Address fields (set to 0 if not using AOUT kludge)
    dd 0    ; header_addr
    dd 0    ; load_addr
    dd 0    ; load_end_addr
    dd 0    ; bss_end_addr
    dd 0    ; entry_addr
    dd 0
    dd 1280
    dd 800
    dd 32

section .data
    align 4096

; initial stack
section .initial_stack, nobits
    align 4

stack_bottom:
    ; 1 MB of uninitialized data for stack
    resb 104856
stack_top:

; kernel entry, main text section
section .text
    global _start
    global MAGIC_HEADER
    global BOOTLOADER_MAGIC


; define _start, aligned by linker.ld script
_start:
    mov esp, stack_top
    extern kmain
    mov eax, BOOTLOADER_MAGIC
    push ebx
    push eax
    call kmain
loop:
    jmp loop
