#ifndef BOOTLOADER_CPU_H_
#define BOOTLOADER_CPU_H_

#include "common.h"

BOOLEAN IsAmd(VOID);
BOOLEAN IsAmdSvmSupported(VOID);
BOOLEAN IsSvmDisabled(VOID);
BOOLEAN IsNRipSaveSupported(VOID);
VOID GetCpuBrandString(CHAR16* brand_string);
UINT32 GetMicrocodeRevision(VOID);
VOID GetAddressWidths(UINT32* physical_width, UINT32* virtual_width);
UINT32 GetSvmRevision(VOID);
VOID PrintCpuInfo(VOID);

#endif
