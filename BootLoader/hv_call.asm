.code

HvCall PROC
    mov rax, rcx
    vmmcall
    ret
HvCall ENDP

END