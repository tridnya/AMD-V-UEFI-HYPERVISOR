#include "cpu.h"

#define SVMDIS    BIT4

#define MSR_VM_CR 0xC0010114
#define MSR_AMD_PATCH_LEVEL 0x0000008B

BOOLEAN
IsAmd(VOID)
{
    UINT32 Eax, Ebx, Ecx, Edx;
    AsmCpuid(0x00000000, &Eax, &Ebx, &Ecx, &Edx);

    return Ebx == SIGNATURE_32('A', 'u', 't', 'h') && Edx == SIGNATURE_32('e', 'n', 't', 'i') && Ecx == SIGNATURE_32('c', 'A', 'M', 'D');
}

BOOLEAN
IsAmdSvmSupported(VOID)
{
    UINT32 Eax, Ebx, Ecx, Edx;
    AsmCpuid(0x80000000, &Eax, &Ebx, &Ecx, &Edx);
    if (Eax < 0x80000001)
        return FALSE;

    AsmCpuid(0x80000001, &Eax, &Ebx, &Ecx, &Edx);
    return (Ecx & BIT2) != 0;
}

BOOLEAN
IsSvmDisabled(VOID)
{
    UINT64 VmCr = AsmReadMsr64(MSR_VM_CR);
    return (VmCr & SVMDIS) != 0;
}

BOOLEAN
IsNRipSaveSupported(VOID)
{
    UINT32 edx;
    AsmCpuid(0x8000000A, NULL, NULL, NULL, &edx);
    return (edx & BIT3) != 0;
}

VOID
GetCpuBrandString(CHAR16* brand_string)
{
    CHAR8 local_brand[49];
    CHAR8* ptr = local_brand;
    UINT32 regs[4];

    for (UINT32 Leaf = 0x80000002; Leaf <= 0x80000004; Leaf++) {
        AsmCpuid(Leaf, &regs[0], &regs[1],&regs[2], &regs[3]);
        CopyMem(ptr, regs, 16);
        ptr += 16;
    }
    local_brand[48] = '\0';

    AsciiStrToUnicodeStrS(local_brand, brand_string, 49);
}

UINT32
GetMicrocodeRevision(VOID)
{
    return (UINT32)AsmReadMsr64(MSR_AMD_PATCH_LEVEL);
}

VOID
GetAddressWidths(UINT32* physical_width, UINT32* virtual_width)
{
    UINT32 Eax, Ebx, Ecx, Edx;
    AsmCpuid(0x80000008, &Eax, &Ebx, &Ecx, &Edx);
    *physical_width = Eax & 0xFF;
    *virtual_width = (Eax >> 8) & 0xFF;
}

UINT32
GetSvmRevision(VOID)
{
    UINT32 Eax, Ebx, Ecx, Edx;
    AsmCpuid(0x8000000A, &Eax, &Ebx, &Ecx, &Edx);
    return Eax & 0xFF;
}

VOID
PrintCpuInfo(VOID)
{
    CHAR16 brand_str[49];
    UINT32 physw = 0, virtw = 0;
    GetCpuBrandString(brand_str);
    GetAddressWidths(&physw, &virtw);

    LOG(L"CPU Brand String: %s", brand_str);
    LOG(L"Microcode Rev:    0x%08X", GetMicrocodeRevision());
    LOG(L"Physical Width:   %d bits", physw);
    LOG(L"Virtual Width:    %d bits", virtw);
}
