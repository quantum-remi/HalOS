[bits 32]

%define REBASE_ADDRESS(x)  (0x7c00 + ((x) - BIOS32_START))

section .text
    global BIOS32_START
    global BIOS32_END
    global bios32_gdt_ptr
    global bios32_gdt_entries
    global bios32_idt_ptr
    global bios32_in_reg16_ptr
    global bios32_out_reg16_ptr
    global bios32_int_number_ptr

BIOS32_START:use32
    pusha
    mov edx, esp
    cli
    xor ecx, ecx
    mov ebx, cr3
    mov cr3, ecx
    lgdt [REBASE_ADDRESS(bios32_gdt_ptr)]
    lidt [REBASE_ADDRESS(bios32_idt_ptr)]
    jmp 0x30:REBASE_ADDRESS(__protected_mode_16)

__protected_mode_16:use16
    mov ax, 0x38
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov eax, cr0
    and al, ~0x01
    mov cr0, eax
    jmp 0x0:REBASE_ADDRESS(__real_mode_16)

__real_mode_16:use16
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x8c00
    sti
    pusha
    mov cx, ss
    push cx
    mov cx, gs
    push cx
    mov cx, fs
    push cx
    mov cx, es
    push cx
    mov cx, ds
    push cx
    pushf
    mov ax, sp
    mov edi, current_esp
    stosw
    mov esp, REBASE_ADDRESS(bios32_in_reg16_ptr)
    popa
    mov sp, 0x9c00
    db 0xCD

bios32_int_number_ptr:
    db 0x00
    mov esp, REBASE_ADDRESS(bios32_out_reg16_ptr)
    add sp, 28
    pushf
    mov cx, ss
    push cx
    mov cx, gs
    push cx
    mov cx, fs
    push cx
    mov cx, es
    push cx
    mov cx, ds
    push cx
    pusha
    mov esi, current_esp
    lodsw
    mov sp, ax
    popf
    pop cx
    mov ds, cx
    pop cx
    mov es, cx
    pop cx
    mov fs, cx
    pop cx
    mov gs, cx
    pop cx
    mov ss, cx
    popa
    mov eax, cr0
    inc eax
    mov cr0, eax
    jmp 0x08:REBASE_ADDRESS(__protected_mode_32)

__protected_mode_32:use32
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov cr3, ebx
    mov esp, edx
    sti
    popa
    ret

__padding:
    db 0x0
    db 0x0
    db 0x0
bios32_gdt_entries:
    resb 64  ; Adjust this if C's NO_GDT_DESCRIPTORS > 8
bios32_gdt_ptr:
    dd 0x00000000
    dd 0x00000000
bios32_idt_ptr:
    dd 0x00000000
    dd 0x00000000
bios32_in_reg16_ptr:
    resw 14
bios32_out_reg16_ptr:
    resw 14  ; Replace placeholder with proper reservation
current_esp:
    dw 0x0000

BIOS32_END: