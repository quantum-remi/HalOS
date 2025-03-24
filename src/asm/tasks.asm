[bits 32]
global switch_to_userspace

switch_to_userspace:
    cli
    mov eax, [esp + 4]     ; Entry point (e.g., 0x8048000)
    mov ebx, [esp + 8]     ; User stack (e.g., 0xBFFFF000)

    ; IRET frame for user mode
    push 0x23              ; SS: User data segment (0x20 | RPL3)
    push ebx               ; ESP
    pushf
    or dword [esp], 0x200  ; Enable interrupts (EFLAGS.IF=1)
    push 0x1B              ; CS: User code segment (0x18 | RPL3)
    push eax               ; EIP

    ; Reload segment registers
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iret                   ; Transition to CPL=3