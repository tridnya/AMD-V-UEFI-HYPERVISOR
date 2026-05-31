#include "seal.h"

#include "awesome_ass_seal.h"

#define SEAL_DISPLAY_W 120
#define SEAL_DISPLAY_H 120

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL* gSealGop = NULL;
STATIC EFI_GRAPHICS_OUTPUT_BLT_PIXEL* gSealBltBuf = NULL;
STATIC UINTN gSealFrame = 0;
STATIC UINT32 gSealDestX = 0;
STATIC UINT32 gSealDestY = 0;
STATIC BOOLEAN gSealActive = FALSE;

STATIC VOID
DrawSealFrame(UINTN frame)
{
    UINT8* src;

    if (!gSealActive || gSealGop == NULL || gSealBltBuf == NULL)
        return;

    src = seal_frames[frame];

    for (UINT32 y = 0; y < SEAL_DISPLAY_H; y++) {
        for (UINT32 x = 0; x < SEAL_DISPLAY_W; x++) {
            UINT32 src_x = x * SEAL_W / SEAL_DISPLAY_W;
            UINT32 src_y = y * SEAL_H / SEAL_DISPLAY_H;
            UINTN src_idx = (src_y * SEAL_W + src_x) * 4;
            UINTN dst_idx = y * SEAL_DISPLAY_W + x;
            gSealBltBuf[dst_idx].Blue     = src[src_idx + 0];
            gSealBltBuf[dst_idx].Green    = src[src_idx + 1];
            gSealBltBuf[dst_idx].Red      = src[src_idx + 2];
            gSealBltBuf[dst_idx].Reserved = 0;
        }
    }

    gSealGop->Blt(gSealGop, gSealBltBuf, EfiBltBufferToVideo,
        0, 0,
        gSealDestX, gSealDestY,
        SEAL_DISPLAY_W, SEAL_DISPLAY_H,
        SEAL_DISPLAY_W * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
}

VOID EFIAPI
SealDrawNextFrame(VOID)
{
    UINTN delay;

    if (!gSealActive)
        return;

    DrawSealFrame(gSealFrame);
    delay = seal_frame_delays[gSealFrame];

    gSealFrame++;
    if (gSealFrame >= SEAL_FRAME_COUNT)
        gSealFrame = 0;

    gBS->Stall((UINTN)(delay * 1000));
}

EFI_INPUT_KEY EFIAPI
WaitForKeyWithSeal(VOID)
{
    EFI_INPUT_KEY Key;
    EFI_STATUS status;

    if (!gSealActive)
        return WaitForKey();

    LOG(L"Press any key to continue...");

    while (TRUE) {
        status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
        if (!EFI_ERROR(status))
            return Key;

        SealDrawNextFrame();
        if (!gSealActive)
            CpuPause();
    }
}

EFI_STATUS EFIAPI
LoadSealGif(EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop)
{
    if (Gop == NULL)
        return EFI_INVALID_PARAMETER;

    UINT32 screen_w = Gop->Mode->Info->HorizontalResolution;
    UINT32 padding = 24;
    UINT32 draw_w = SEAL_DISPLAY_W;
    UINT32 draw_h = SEAL_DISPLAY_H;

    UINT32 dest_x = screen_w - draw_w - padding;
    UINT32 dest_y = padding;

    UINTN buf_size = draw_w * draw_h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL* blt_buf = NULL;
    EFI_STATUS status = gBS->AllocatePool(EfiBootServicesData, buf_size, (VOID**)&blt_buf);
    if (EFI_ERROR(status))
        return status;

    for (UINTN frame = 0; frame < SEAL_FRAME_COUNT; frame++) {
        UINT8* src = seal_frames[frame];

        for (UINT32 y = 0; y < draw_h; y++) {
            for (UINT32 x = 0; x < draw_w; x++) {
                UINT32 src_x = x * SEAL_W / draw_w;
                UINT32 src_y = y * SEAL_H / draw_h;
                UINTN src_idx = (src_y * SEAL_W + src_x) * 4;
                UINTN dst_idx = y * draw_w + x;
                blt_buf[dst_idx].Blue     = src[src_idx + 0];
                blt_buf[dst_idx].Green    = src[src_idx + 1];
                blt_buf[dst_idx].Red      = src[src_idx + 2];
                blt_buf[dst_idx].Reserved = 0;
            }
        }

        Gop->Blt(Gop, blt_buf, EfiBltBufferToVideo,
            0, 0,
            dest_x, dest_y,
            draw_w, draw_h,
            draw_w * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

        gBS->Stall((UINTN)(seal_frame_delays[frame] * 1000));
    }

    gBS->FreePool(blt_buf);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
InitializeSeal(VOID)
{
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    UINT32 padding = 24;
    UINTN buf_size;

    status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop);
    if (EFI_ERROR(status))
        ERROR(L"Failed to initialize gop: %r", status);
    else
    {
       gSealGop = gop;
       gSealDestX = gop->Mode->Info->HorizontalResolution - SEAL_DISPLAY_W - padding;
       gSealDestY = padding;
       gSealFrame = 0;

       buf_size = SEAL_DISPLAY_W * SEAL_DISPLAY_H * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
       status = gBS->AllocatePool(EfiBootServicesData, buf_size, (VOID**)&gSealBltBuf);
       if (EFI_ERROR(status))
           ERROR(L"Failed to initialize seal: %r", status);
       else {
           gSealActive = TRUE;
           DrawSealFrame(gSealFrame);
       }
    }

    return status;
}

VOID EFIAPI
StopSeal(VOID)
{
    gSealActive = FALSE;

    if (gSealBltBuf != NULL) {
        gBS->FreePool(gSealBltBuf);
        gSealBltBuf = NULL;
    }

    gSealGop = NULL;
}
