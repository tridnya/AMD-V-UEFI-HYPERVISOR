#include "common.h"
#include "cpu.h"
#include "seal.h"
#include "svm.h"
#include "windows_loader.h"

extern VOID GuestEntry(VOID);

EFI_HANDLE gImageHandle = NULL;
EFI_SYSTEM_TABLE* gSystemTable = NULL;
BOOLEAN gWindowsStarted = FALSE;
volatile UINT64 gLastExitCode = 0;
volatile UINT64 gLastRip = 0;
volatile UINT64 gLastExitInfo1 = 0;
volatile UINT64 gLastExitInfo2 = 0;

STATIC EFI_STATUS EFIAPI
InitializeChecks(VOID)
{
    EFI_STATUS status = EFI_UNSUPPORTED;

    if (!IsAmd())
    {
        ERROR(L"Non-AMD CPU");
        status = EFI_UNSUPPORTED;
        return status;
    }
    LOG(L"CPU supported");

    if (!IsAmdSvmSupported())
    {
        ERROR(L"AMD SVM not supported");
        return status;
    }
    LOG(L"CPU supports SVM");

    if (IsSvmDisabled()) {
        ERROR(L"SVM is disabled in bios");
        return status;
    }
    LOG(L"SVM enabled in bios");

    if (!IsNRipSaveSupported()) {
        ERROR(L"NRIP save not supported");
        return status;
    }
    LOG(L"NRIP save supported");

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    gImageHandle = ImageHandle;
    gSystemTable = SystemTable;

    EFI_STATUS status = EFI_UNSUPPORTED;
    PAGES p_pages;
    GSTACK guest_stack;
    EFI_PHYSICAL_ADDRESS msrpm_pa = 0;
    LOG(L"Bootloader started");

    ZeroMem(&p_pages, sizeof(p_pages));
    ZeroMem(&guest_stack, sizeof(guest_stack));

    status = InitializeChecks();
    if (EFI_ERROR(status))
        goto exit;

    status = InitializeSeal();
    LOG(L"Initialized seal successfully");

    EnableSvm();
    LOG(L"SVM enabled");

    status = AllocateSvmPages(&p_pages, &guest_stack);
    if (EFI_ERROR(status))
        goto exit;

    LOG(L"Successfully allocated SVM pages");

    status = AllocateMsrPermBitmap(&msrpm_pa);
    if (EFI_ERROR(status))
        goto exit;

    LOG(L"Successfully allocated MSR bitmap");

    status = InitializeVmcb(p_pages.vmcb_page, msrpm_pa, guest_stack.guest_rsp, SvmBootWindows);
    if (EFI_ERROR(status))
        goto exit;
    LOG(L"Successfully initialzed VMCB");

    PrintCpuInfo();

    LOG(L"Bootloader successfully initialized");
    status = EFI_SUCCESS;
    LOG(L"Key accepted");

    LOG(L"Deploying guest boot path...");
    status = RunGuestRuntime(&p_pages, SvmBootWindows);
    ERROR(L"RunGuest returned: %r", status);

    /*
        If Windows is booting under the VMM correctly,
        RunGuest should not normally return here.
    */
    goto exit;

exit: // leaks are intentional
    LOG(L"Exiting with status: %r", status);
    WaitForKeyWithSeal();
    return status;
}
