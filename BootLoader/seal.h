#ifndef BOOTLOADER_SEAL_H_
#define BOOTLOADER_SEAL_H_

#include "common.h"
#include <Protocol/GraphicsOutput.h>

EFI_STATUS EFIAPI LoadSealGif(EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop);
EFI_STATUS EFIAPI InitializeSeal(VOID);
VOID EFIAPI SealDrawNextFrame(VOID);
EFI_INPUT_KEY EFIAPI WaitForKeyWithSeal(VOID);
VOID EFIAPI StopSeal(VOID);

#endif
