.code

HvCall PROC
    mov     rax, rcx        ; call number ? RAX
    db      0Fh, 01h, 0D9h  ; vmmcall
    ret
HvCall ENDP

END