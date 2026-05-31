BITS 64

section .text

global GuestEntry

GuestEntry:
    mov     rax, 1
    db      0x0F, 0x01, 0xD9    ; vmmcall

    mov     rbx, rax

    mov     rax, 2
    db      0x0F, 0x01, 0xD9    ; vmmcall

    hlt

.hang:
    hlt
    jmp     .hang