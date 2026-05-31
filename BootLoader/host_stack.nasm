BITS 64

section .text

extern RunGuest
global RunGuestOnHostStack

; Windows x64 ABI:
; RCX = vmcb_pa
; RDX = hsave_page
; R8  = Mode
; R9  = host_rsp
RunGuestOnHostStack:
    mov     r10, rsp
    mov     rsp, r9
    and     rsp, 0FFFFFFFFFFFFFFF0h
    sub     rsp, 30h
    mov     [rsp + 20h], r10

    call    RunGuest

    mov     r10, [rsp + 20h]
    add     rsp, 30h
    mov     rsp, r10
    ret
