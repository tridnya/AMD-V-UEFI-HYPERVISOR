#ifndef BOOTLOADER_VMCB_H_
#define BOOTLOADER_VMCB_H_

#include "common.h"

#define VMCB_CONTROL_OFFSET 0x000
#define VMCB_STATE_OFFSET   0x400

#define SVM_EXIT_INVALID  0xFFFFFFFFFFFFFFFFULL
#define SVM_EXIT_EXCP_BASE 0x40
#define SVM_EXIT_HLT      0x78
#define SVM_EXIT_VMMCALL  0x81
#define SVM_EXIT_MSR              0x7C
#define SVM_EXIT_INIT     0x72

#define HV_CALL_GET_VERSION  0x1
#define HV_CALL_PING        0x2

#define HV_PONG             0xDEADBEEF
#define HV_VERSION           0x00010000

#define VMCB_ATTR_CODE64 0x0A9B
#define VMCB_ATTR_DATA   0x0C93
#define VMCB_ATTR_TSS64  0x008B

#define SVM_INTERCEPT_MISC1_INTR            BIT0
#define SVM_INTERCEPT_MISC1_NMI             BIT1
#define SVM_INTERCEPT_MISC1_SMI             BIT2
#define SVM_INTERCEPT_MISC1_INIT            BIT3
#define SVM_INTERCEPT_MISC1_VINTR           BIT4
#define SVM_INTERCEPT_MISC1_CR0_SEL_WRITE   BIT5
#define SVM_INTERCEPT_MISC1_SIDT            BIT6
#define SVM_INTERCEPT_MISC1_SGDT            BIT7
#define SVM_INTERCEPT_MISC1_SLDT            BIT8
#define SVM_INTERCEPT_MISC1_STR             BIT9
#define SVM_INTERCEPT_MISC1_LIDT            BIT10
#define SVM_INTERCEPT_MISC1_LGDT            BIT11
#define SVM_INTERCEPT_MISC1_LLDT            BIT12
#define SVM_INTERCEPT_MISC1_LTR             BIT13
#define SVM_INTERCEPT_MISC1_RDTSC           BIT14
#define SVM_INTERCEPT_MISC1_RDPMC           BIT15
#define SVM_INTERCEPT_MISC1_PUSHF           BIT16
#define SVM_INTERCEPT_MISC1_POPF            BIT17
#define SVM_INTERCEPT_MISC1_CPUID           BIT18
#define SVM_INTERCEPT_MISC1_RSM             BIT19
#define SVM_INTERCEPT_MISC1_IRET            BIT20
#define SVM_INTERCEPT_MISC1_INTn            BIT21
#define SVM_INTERCEPT_MISC1_INVD            BIT22
#define SVM_INTERCEPT_MISC1_PAUSE           BIT23
#define SVM_INTERCEPT_MISC1_HLT             BIT24
#define SVM_INTERCEPT_MISC1_INVLPG          BIT25
#define SVM_INTERCEPT_MISC1_INVLPGA         BIT26
#define SVM_INTERCEPT_MISC1_IOIO_PROT       BIT27
#define SVM_INTERCEPT_MISC1_MSR_PROT        BIT28
#define SVM_INTERCEPT_MISC1_TASK_SWITCH     BIT29
#define SVM_INTERCEPT_MISC1_FERR_FREEZE     BIT30
#define SVM_INTERCEPT_MISC1_SHUTDOWN        BIT31

#define SVM_INTERCEPT_MISC2_VMRUN           BIT0
#define SVM_INTERCEPT_MISC2_VMMCALL         BIT1
#define SVM_INTERCEPT_MISC2_VMLOAD          BIT2
#define SVM_INTERCEPT_MISC2_VMSAVE          BIT3
#define SVM_INTERCEPT_MISC2_STGI            BIT4
#define SVM_INTERCEPT_MISC2_CLGI            BIT5
#define SVM_INTERCEPT_MISC2_SKINIT          BIT6
#define SVM_INTERCEPT_MISC2_RDTSCP          BIT7
#define SVM_INTERCEPT_MISC2_ICEBP           BIT8
#define SVM_INTERCEPT_MISC2_WBINVD          BIT9
#define SVM_INTERCEPT_MISC2_MONITOR         BIT10
#define SVM_INTERCEPT_MISC2_MWAIT           BIT11
#define SVM_INTERCEPT_MISC2_MWAIT_ARMED     BIT12
#define SVM_INTERCEPT_MISC2_XSETBV          BIT13
#define SVM_INTERCEPT_MISC2_RDPRU           BIT14
#define SVM_INTERCEPT_MISC2_EFER_WRITE_TRAP BIT15
#define SVM_INTERCEPT_MISC2_CR_WRITE_TRAP   BIT16

#define SVM_INTERCEPT_MISC3_INVLPGB         BIT0
#define SVM_INTERCEPT_MISC3_INVLPGB_ILLEGAL BIT1
#define SVM_INTERCEPT_MISC3_PCID            BIT2
#define SVM_INTERCEPT_MISC3_MCOMMIT         BIT3
#define SVM_INTERCEPT_MISC3_TLBSYNC         BIT4

#define SVM_CTRL_TSC_OFFSET_ENABLE          BIT0

#define SVM_TLB_CONTROL_DO_NOTHING          0x00
#define SVM_TLB_CONTROL_FLUSH_ALL           0x01
#define SVM_TLB_CONTROL_FLUSH_GUEST         0x03
#define SVM_TLB_CONTROL_FLUSH_GUEST_NONGLOBAL 0x07

#define SVM_CTRL_VINTR_MASK_ALL             BIT24
#define SVM_CTRL_VINTR_VGIF                 BIT25

#define SVM_NP_ENABLE_NP                    BIT0
#define SVM_NP_ENABLE_SEV                   BIT1
#define SVM_NP_ENABLE_SEV_ES                BIT4

#define SVM_CLEAN_INTERCEPTS                BIT0
#define SVM_CLEAN_IOMSRPM                   BIT1
#define SVM_CLEAN_ASID                      BIT2
#define SVM_CLEAN_TPR                       BIT3
#define SVM_CLEAN_NP                        BIT4
#define SVM_CLEAN_CONTROL_REGS              BIT5
#define SVM_CLEAN_CR2                       BIT6
#define SVM_CLEAN_LBR                       BIT7
#define SVM_CLEAN_AVIC                      BIT8
#define SVM_CLEAN_CET                       BIT9

#define MSR_EFER_SVME             (1ULL << 12)

typedef struct {
    UINT16   InterceptCrRead;
    UINT16   InterceptCrWrite;
    UINT16   InterceptDrRead;
    UINT16   InterceptDrWrite;
    UINT32   InterceptException;
    UINT32   InterceptMisc1;
    UINT32   InterceptMisc2;
    UINT32   InterceptMisc3;
    UINT8    Reserved0[0x03C - 0x018];
    UINT16   PauseFilterThreshold;
    UINT16   PauseFilterCount;
    UINT64   IopmBasePa;
    UINT64   MsrpmBasePa;
    UINT64   TscOffset;
    UINT32   GuestAsid;
    UINT8    TlbControl;
    UINT8    Reserved1[3];
    UINT64   VIntr;
    UINT64   InterruptShadow;
    UINT64   ExitCode;
    UINT64   ExitInfo1;
    UINT64   ExitInfo2;
    UINT64   ExitIntInfo;
    UINT64   NpEnable;
    UINT64   AvicApicBar;
    UINT64   GhcbPa;
    UINT64   EventInj;
    UINT64   NCr3;
    UINT64   LbrVirtEnable;
    UINT32   VmcbCleanBits;
    UINT32   Reserved2;
    UINT64   NRip;
    UINT8    NumBytesFetched;
    UINT8    FetchedBytes[15];
    UINT64   AvicApicBackingPagePa;
    UINT64   Reserved3;
    UINT64   AvicLogicalTablePa;
    UINT64   AvicPhysicalTablePa;
    UINT64   Reserved4;
    UINT64   VmsaPtr;
    UINT8    Reserved5[0x400 - 0x110];
} VMCB_CONTROL_AREA;

typedef struct {
    UINT16  Selector;
    UINT16  Attributes;
    UINT32  Limit;
    UINT64  Base;
} VMCB_SEGMENT_REGISTER;

typedef struct {
    UINT16  Reserved0;
    UINT16  Reserved1;
    UINT32  Limit;
    UINT64  Base;
} VMCB_DESCRIPTOR_TABLE_REGISTER;

typedef struct VMCB_STATE_SAVE_AREA {
    VMCB_SEGMENT_REGISTER           Es;
    VMCB_SEGMENT_REGISTER           Cs;
    VMCB_SEGMENT_REGISTER           Ss;
    VMCB_SEGMENT_REGISTER           Ds;
    VMCB_SEGMENT_REGISTER           Fs;
    VMCB_SEGMENT_REGISTER           Gs;
    VMCB_DESCRIPTOR_TABLE_REGISTER  Gdtr;
    VMCB_SEGMENT_REGISTER           Ldtr;
    VMCB_DESCRIPTOR_TABLE_REGISTER  Idtr;
    VMCB_SEGMENT_REGISTER           Tr;
    UINT8   Reserved0[0xCB - 0xA0];
    UINT8   Cpl;
    UINT32  Reserved1;
    UINT64  Efer;
    UINT8   Reserved2[0x148 - 0xD8];
    UINT64  Cr4;
    UINT64  Cr3;
    UINT64  Cr0;
    UINT64  Dr7;
    UINT64  Dr6;
    UINT64  Rflags;
    UINT64  Rip;
    UINT8   Reserved3[0x1D8 - 0x180];
    UINT64  Rsp;
    UINT64  SCet;
    UINT64  Ssp;
    UINT64  IsstAddr;
    UINT64  Rax;
    UINT64  Star;
    UINT64  Lstar;
    UINT64  Cstar;
    UINT64  Sfmask;
    UINT64  KernelGsBase;
    UINT64  SysenterCs;
    UINT64  SysenterEsp;
    UINT64  SysenterEip;
    UINT64  Cr2;
    UINT64  Pat;                        // 0x248 — Page Attribute Table
    UINT8   Reserved4[0x268 - 0x250];   // 0x250 to 0x268
    UINT64  DbgCtl;
    UINT64  BrFrom;
    UINT64  BrTo;
    UINT64  LastExcepFrom;
    UINT64  LastExcepTo;
    UINT8   Reserved5[0x2E0 - 0x290];
    UINT64  SpecCtrl;
} VMCB_STATE_SAVE_AREA;

typedef struct _GUEST_CONTEXT {
    UINT64 Rax;
    UINT64 Rcx;
    UINT64 Rdx;
    UINT64 Rbx;
    UINT64 Rsp;
    UINT64 Rbp;
    UINT64 Rsi;
    UINT64 Rdi;
    UINT64 R8;
    UINT64 R9;
    UINT64 R10;
    UINT64 R11;
    UINT64 R12;
    UINT64 R13;
    UINT64 R14;
    UINT64 R15;
} GUEST_CONTEXT;

#endif
