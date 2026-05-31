#include "windows_loader.h"

#include <Library/DevicePathLib.h>
#include <Protocol/SimpleFileSystem.h>
#include "seal.h"

#define WINDOWS_BOOT_PATH L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi"

EFI_STATUS EFIAPI
LoadWindows(EFI_HANDLE image_handle)
{
    if (!image_handle)
        return EFI_INVALID_PARAMETER;

    EFI_STATUS status = EFI_UNSUPPORTED;
    UINTN handle_count = 0;
    EFI_HANDLE* handle_buffer = NULL;
    EFI_HANDLE windows_image_handle = NULL;
    EFI_DEVICE_PATH_PROTOCOL* device_path = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = NULL;
    EFI_FILE_PROTOCOL* root = NULL;
    EFI_FILE_PROTOCOL* file = NULL;

    status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &handle_count, &handle_buffer);
    if (EFI_ERROR(status)) {
        return status;
    }

    for (UINTN i = 0; i < handle_count; i++) {
        fs = NULL;
        root = NULL;
        file = NULL;
        windows_image_handle = NULL;

        status = gBS->HandleProtocol(handle_buffer[i], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&fs);
        if (EFI_ERROR(status))
            continue;

        status = fs->OpenVolume(fs, &root);
        if (EFI_ERROR(status))
            continue;

        status = root->Open(root, &file, WINDOWS_BOOT_PATH, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status)) {
            root->Close(root);
            continue;
        }

        file->Close(file);
        root->Close(root);

        device_path = FileDevicePath(handle_buffer[i], WINDOWS_BOOT_PATH);
        if (device_path == NULL) {
            gBS->FreePool(handle_buffer);
            return EFI_OUT_OF_RESOURCES;
        }

        status = gBS->LoadImage(TRUE, image_handle, device_path, NULL, 0, &windows_image_handle);
        gBS->FreePool(device_path);

        if (!EFI_ERROR(status))
            break;
    }

    gBS->FreePool(handle_buffer);
    if (windows_image_handle == NULL)
        return EFI_NOT_FOUND;

    LOG(L"Starting Windows");
    WaitForKeyWithSeal();
    StopSeal();
    gWindowsStarted = TRUE;
    status = gBS->StartImage(windows_image_handle, NULL, NULL);
    LOG(L"StartImage returned: %r", status);
    return status;
}
