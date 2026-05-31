BITS 64

section .text

extern GuestBootEntry
global GuestBootTrampoline

GuestBootTrampoline:
    sub     rsp, 28h
    call    GuestBootEntry
    add     rsp, 28h

.hang:
    pause
    jmp     .hang
