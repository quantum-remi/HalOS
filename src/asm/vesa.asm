; ; In vesa_init.asm
[bits 32]
global vesa_force_mode
vesa_force_mode:
;     pusha
;     mov ax, 0x4F02       ; VBE function 02h - Set Video Mode
;     mov bx, 0x4115       | 0x4000  ; Mode 0x115 (1024x768x32) + linear buffer
;     int 0x10
;     popa
;     ret