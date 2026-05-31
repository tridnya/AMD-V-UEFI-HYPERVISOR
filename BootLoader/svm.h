#ifndef BOOTLOADER_SVM_H_
#define BOOTLOADER_SVM_H_

#include "vmcb.h"

typedef struct AMDV_PAGES {
    EFI_PHYSICAL_ADDRESS     hs_page, vmcb_page, host_stack, host_pml4, runtime_loop;
    UINT64 host_rsp;
} PAGES;

typedef struct GUEST_STACK {
    EFI_PHYSICAL_ADDRESS guest_stack;
    UINT64 guest_rsp;
} GSTACK;

typedef enum SVM_BOOT_MODE {
    SvmBootTestGuest,
    SvmBootWindows
} SVM_BOOT_MODE;

VOID EnableSvm(VOID);
VOID DisableSvm(VOID);
EFI_STATUS EFIAPI AllocateSvmPages(PAGES* pages, GSTACK* guest_stack);
EFI_STATUS EFIAPI AllocateMsrPermBitmap(EFI_PHYSICAL_ADDRESS* msrpm_pa);
EFI_STATUS EFIAPI InitializeVmcb(EFI_PHYSICAL_ADDRESS vmcb_page, EFI_PHYSICAL_ADDRESS msrpm_pa, UINT64 guest_rsp, SVM_BOOT_MODE Mode);
EFI_STATUS RunGuest(EFI_PHYSICAL_ADDRESS vmcb_pa, EFI_PHYSICAL_ADDRESS hsave_page, SVM_BOOT_MODE Mode);
EFI_STATUS RunGuestOnHostStack(EFI_PHYSICAL_ADDRESS vmcb_pa, EFI_PHYSICAL_ADDRESS hsave_page, SVM_BOOT_MODE Mode, UINT64 host_rsp);
EFI_STATUS EFIAPI RunGuestRuntime(PAGES* pages, SVM_BOOT_MODE Mode);

#endif
