#ifndef BOOTLOADER_COMMON_H_
#define BOOTLOADER_COMMON_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#define LOG(fmt, ...)   Print(L"[#] " fmt L"\n", ##__VA_ARGS__)
#define WARN(fmt, ...)  Print(L"[!] " fmt L"\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) Print(L"[x] " fmt L"\n", ##__VA_ARGS__)

extern BOOLEAN gWindowsStarted;
extern volatile UINT64 gLastExitCode;
extern volatile UINT64 gLastRip;
extern volatile UINT64 gLastExitInfo1;
extern volatile UINT64 gLastExitInfo2;

STATIC EFI_INPUT_KEY
WaitForKey(VOID)
{
    EFI_INPUT_KEY Key;
    UINTN Index;

    LOG(L"Press any key to continue...");
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    return Key;
}

#endif
