DEFAULT REL
SECTION .text

global AsmReadSegAttributes
AsmReadSegAttributes:
    xor     eax, eax
    lar     eax, ecx
    shr     eax, 8
    and     eax, 0x0000F0FF
    ret

global AsmReadSegLimit
AsmReadSegLimit:
    xor     eax, eax
    lsl     eax, ecx
    ret