#include "svm.h"

#include <Library/DebugLib.h>

#define EFER_SVME       BIT12

#define MSR_VM_HSAVE_PA 0xC0010117
#define MSR_EFER        0xC0000080
#define MSR_IA32_FS_BASE   0xC0000100
#define MSR_IA32_GS_BASE   0xC0000101
#define GUEST_STACK_PAGES 16
#define HOST_STACK_PAGES 16
#define HOST_PT_PAGES     6
#define HOST_IDENTITY_SIZE 0x100000000ULL
#define PAGE_SIZE_2M      0x200000ULL
#define PAGE_PRESENT      BIT0
#define PAGE_RW           BIT1
#define PAGE_2M           BIT7

extern VOID GuestBootTrampoline(VOID);
extern UINT8 RuntimeVmLoopTemplate[];
extern UINT8 RuntimeVmLoopTemplateEnd[];
extern VOID AsmVmRun(UINT64 VmcbPa, GUEST_CONTEXT *Context);
UINT16 EFIAPI AsmReadSegAttributes(UINT16 Selector);
UINT32 EFIAPI AsmReadSegLimit(UINT16 Selector);

typedef VOID (EFIAPI *RUNTIME_VM_LOOP)(EFI_PHYSICAL_ADDRESS VmcbPa, EFI_PHYSICAL_ADDRESS HsavePage, EFI_PHYSICAL_ADDRESS HostPml4, UINT64 HostRsp);

STATIC VOID
RecordFatalExit(VMCB_CONTROL_AREA *Control, VMCB_STATE_SAVE_AREA *State)
{
    gLastExitCode = Control->ExitCode;
    gLastRip = State->Rip;
    gLastExitInfo1 = Control->ExitInfo1;
    gLastExitInfo2 = Control->ExitInfo2;
}

VOID
EnableSvm(VOID)
{
    UINT64 efer = AsmReadMsr64(MSR_EFER);
    efer |= EFER_SVME | BIT11;  // BIT11 = NXE, ensure it survives host save/restore
    AsmWriteMsr64(MSR_EFER, efer);
}

VOID
DisableSvm(VOID)
{
    UINT64 efer = AsmReadMsr64(MSR_EFER);
    AsmWriteMsr64(MSR_EFER, efer & ~((UINT64)BIT12));  // clear SVME only
    LOG(L"SVM disabled, EFER=0x%llx", AsmReadMsr64(MSR_EFER));
}

STATIC VOID
CaptureSegmentRegister(VMCB_SEGMENT_REGISTER *Seg, UINT16 Selector)
{
    Seg->Selector   = Selector;
    Seg->Attributes = AsmReadSegAttributes(Selector);
    Seg->Limit      = AsmReadSegLimit(Selector);
    Seg->Base       = 0;
}

STATIC EFI_STATUS
AllocateLowRuntimePages(EFI_MEMORY_TYPE MemoryType, UINTN PageCount, EFI_PHYSICAL_ADDRESS* Base)
{
    if (Base == NULL || PageCount == 0)
        return EFI_INVALID_PARAMETER;

    EFI_PHYSICAL_ADDRESS Address = 0xFFFFFFFFULL;
    EFI_STATUS status = gBS->AllocatePages(AllocateMaxAddress, MemoryType, PageCount, &Address);
    if (EFI_ERROR(status))
        return status;

    *Base = Address;
    ZeroMem((VOID*)(UINTN)*Base, PageCount * EFI_PAGE_SIZE);
    return status;
}

EFI_STATUS EFIAPI
AllocatePage(EFI_PHYSICAL_ADDRESS* page)
{
    return AllocateLowRuntimePages(EfiRuntimeServicesData, 1, page);
}

STATIC VOID
BuildHostPageTables(EFI_PHYSICAL_ADDRESS HostPml4)
{
    UINT64 *Pml4 = (UINT64*)(UINTN)HostPml4;
    UINT64 *Pdpt = (UINT64*)((UINTN)HostPml4 + EFI_PAGE_SIZE);
    UINT64 *Pd = (UINT64*)((UINTN)HostPml4 + (2 * EFI_PAGE_SIZE));

    Pml4[0] = ((UINT64)(UINTN)Pdpt) | PAGE_PRESENT | PAGE_RW;

    for (UINTN PdptIndex = 0; PdptIndex < (HOST_IDENTITY_SIZE / (512 * PAGE_SIZE_2M)); PdptIndex++) {
        UINT64 *CurrentPd = Pd + (PdptIndex * 512);
        Pdpt[PdptIndex] = ((UINT64)(UINTN)CurrentPd) | PAGE_PRESENT | PAGE_RW;

        for (UINTN PdIndex = 0; PdIndex < 512; PdIndex++) {
            UINT64 Phys = ((PdptIndex * 512) + PdIndex) * PAGE_SIZE_2M;
            CurrentPd[PdIndex] = Phys | PAGE_PRESENT | PAGE_RW | PAGE_2M;
        }
    }
}

STATIC EFI_STATUS
AllocateRuntimeVmLoop(PAGES* pages)
{
    UINTN RuntimeLoopSize = (UINTN)RuntimeVmLoopTemplateEnd - (UINTN)RuntimeVmLoopTemplate;
    UINTN RuntimeLoopPages = (RuntimeLoopSize + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;

    EFI_STATUS status = AllocateLowRuntimePages(EfiRuntimeServicesCode, RuntimeLoopPages, &pages->runtime_loop);
    if (EFI_ERROR(status))
        return status;

    CopyMem((VOID*)(UINTN)pages->runtime_loop, (VOID*)(UINTN)RuntimeVmLoopTemplate, RuntimeLoopSize);

    LOG(L"Runtime VM loop: 0x%lx", pages->runtime_loop);
    LOG(L"Runtime VM loop size: 0x%lx", RuntimeLoopSize);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
AllocateSvmPages(PAGES* pages, GSTACK* guest_stack)
{
    EFI_STATUS status = EFI_UNSUPPORTED;

    status = AllocatePage(&pages->hs_page);
    if (EFI_ERROR(status)) {
        ERROR(L"Failed to allocate host save: %r", status);
        return status;
    }

    LOG(L"Hsave Page: 0x%lx", pages->hs_page);
    AsmWriteMsr64(MSR_VM_HSAVE_PA, pages->hs_page);
 
    UINT64 test_read = AsmReadMsr64(MSR_VM_HSAVE_PA);
    LOG(L"VM_HSAVE_PA: 0x%lx", test_read);

    status = AllocateLowRuntimePages(EfiRuntimeServicesData, HOST_STACK_PAGES, &pages->host_stack);
    if (EFI_ERROR(status)) {
        ERROR(L"Failed to allocate host stack: %r", status);
        return status;
    }

    pages->host_rsp = pages->host_stack + (HOST_STACK_PAGES * EFI_PAGE_SIZE);
    LOG(L"Host stack: 0x%lx", pages->host_stack);
    LOG(L"Host RSP: 0x%lx", pages->host_rsp);

    status = AllocateRuntimeVmLoop(pages);
    if (EFI_ERROR(status)) {
        ERROR(L"Failed to allocate runtime VM loop: %r", status);
        return status;
    }

    status = AllocateLowRuntimePages(EfiRuntimeServicesData, HOST_PT_PAGES, &pages->host_pml4);
    if (EFI_ERROR(status)) {
        ERROR(L"Failed to allocate host page tables: %r", status);
        return status;
    }

    BuildHostPageTables(pages->host_pml4);
    LOG(L"Host PML4: 0x%lx", pages->host_pml4);

    status = gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, GUEST_STACK_PAGES, &guest_stack->guest_stack);
    if (EFI_ERROR(status)) {
        ERROR(L"Failed to allocate guest stack: %r", status);
        return status;
    }
    ZeroMem((VOID*)(UINTN)guest_stack->guest_stack, GUEST_STACK_PAGES * EFI_PAGE_SIZE);

    LOG(L"Guest stack: 0x%lx", guest_stack->guest_stack);

    guest_stack->guest_rsp = guest_stack->guest_stack + (GUEST_STACK_PAGES * EFI_PAGE_SIZE) - 0x8;
    LOG(L"Guest RSP: 0x%lx", guest_stack->guest_rsp);
    LOG(L"Guest RIP: 0x%lx", (UINT64)(UINTN)&GuestBootTrampoline);

    status = AllocatePage(&pages->vmcb_page);
    if (EFI_ERROR(status)) {
        ERROR(L"Failed to allocate VMCB: %r", status);
        return status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
AllocateMsrPermBitmap(EFI_PHYSICAL_ADDRESS* msrpm_pa)
{
    EFI_STATUS status = gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 4, msrpm_pa);
    if (EFI_ERROR(status))
    {
        ERROR(L"Failed to allocate MSR bitmap: %r", status);
        return status;
    }
    SetMem((VOID*)(UINTN)*msrpm_pa, 4 * EFI_PAGE_SIZE, 0xFF);

    return status;
}

EFI_STATUS EFIAPI
InitializeVmcb(EFI_PHYSICAL_ADDRESS vmcb_page, EFI_PHYSICAL_ADDRESS msrpm_pa, UINT64 guest_rsp, SVM_BOOT_MODE Mode)
{
    if (!vmcb_page || !guest_rsp)
        return EFI_INVALID_PARAMETER;

    ASSERT((vmcb_page & (EFI_PAGE_SIZE - 1)) == 0);

    VMCB_CONTROL_AREA *control = (VMCB_CONTROL_AREA*)((UINTN)vmcb_page + VMCB_CONTROL_OFFSET);
    VMCB_STATE_SAVE_AREA *state = (VMCB_STATE_SAVE_AREA*)((UINTN)vmcb_page + VMCB_STATE_OFFSET);

    ZeroMem(control, sizeof(*control));
    ZeroMem(state,   sizeof(*state));

    /*
        Control area:
        - VMRUN intercept is required.
        - VMMCALL intercept gives us hypercalls.
        - Windows boot mode keeps the intercept policy minimal.
    */
    control->InterceptMisc2 =
        SVM_INTERCEPT_MISC2_VMRUN |
        SVM_INTERCEPT_MISC2_VMMCALL;

    control->InterceptException = 0;

    if (Mode == SvmBootTestGuest)
        control->InterceptMisc1 = SVM_INTERCEPT_MISC1_HLT;
    else
        control->InterceptMisc1 = 0;

/*
    Enable MSR permission bitmap and intercept EFER writes.
    Without this, Windows clears SVME from EFER during boot,
    causing vmmcall to fault as #UD in the guest.
    
    MSR 0xC0000080 (EFER) lives in MSR range 2 of the bitmap.
    Range 2 covers 0xC0000000-0xC0001FFF.
    Offset = ((0xC0000080 - 0xC0000000) / 4) * 2 = 0x40 bytes in, bit 1 = write.
    Each MSR takes 2 bits: bit 0 = read intercept, bit 1 = write intercept.
    Base of range 2 in MSRPM is at byte offset 0x800.
*/
    control->MsrpmBasePa = msrpm_pa;
    control->InterceptMisc1 |= SVM_INTERCEPT_MISC1_MSR_PROT;

    UINT8 *msrpm = (UINT8*)(UINTN)msrpm_pa;
    SetMem(msrpm, 2 * EFI_PAGE_SIZE, 0x00);

// Re-enable write intercept for EFER only (bit 1 at byte 0x820)
    msrpm[0x820] |= 0x02;
    
    control->GuestAsid      = 1;
    control->TlbControl     = SVM_TLB_CONTROL_FLUSH_GUEST;
    control->VmcbCleanBits  = 0;

    /*
        Capture selectors, then force known-good flat long-mode state.
        Dynamic descriptor decoding can be wrong/weird under firmware/VMware.
    */
    CaptureSegmentRegister(&state->Es, AsmReadEs());
    CaptureSegmentRegister(&state->Cs, AsmReadCs());
    CaptureSegmentRegister(&state->Ss, AsmReadSs());
    CaptureSegmentRegister(&state->Ds, AsmReadDs());

    CaptureSegmentRegister(&state->Fs, AsmReadFs());
    CaptureSegmentRegister(&state->Gs, AsmReadGs());

    state->Fs.Base = AsmReadMsr64(MSR_IA32_FS_BASE);
    state->Gs.Base = AsmReadMsr64(MSR_IA32_GS_BASE);

    /*
        Descriptor tables can be copied from host.
    */
    IA32_DESCRIPTOR gdtr;
    IA32_DESCRIPTOR idtr;

    AsmReadGdtr(&gdtr);
    state->Gdtr.Base  = (UINT64)(UINTN)gdtr.Base;
    state->Gdtr.Limit = gdtr.Limit;

    AsmReadIdtr(&idtr);
    state->Idtr.Base  = (UINT64)(UINTN)idtr.Base;
    state->Idtr.Limit = idtr.Limit;

    /*
        Do not trust dynamic LDTR/TR during bring-up.
        LDTR can be null.
        TR needs a sane present 64-bit TSS-like state.
    */
    state->Ldtr.Selector   = 0;
    state->Ldtr.Attributes = 0;
    state->Ldtr.Limit      = 0;
    state->Ldtr.Base       = 0;

    state->Tr.Selector     = 0x40;
    state->Tr.Attributes   = VMCB_ATTR_TSS64;
    state->Tr.Limit        = 0x67;
    state->Tr.Base         = 0;

    if (state->Ss.Selector == 0)
        state->Ss.Selector = 0x20;
    if (state->Ds.Selector == 0)
        state->Ds.Selector = state->Ss.Selector;
    if (state->Es.Selector == 0)
        state->Es.Selector = state->Ss.Selector;

    /*
        Known-good flat 64-bit segment attributes.
    */
    state->Cs.Attributes = VMCB_ATTR_CODE64;

    state->Ss.Attributes = VMCB_ATTR_DATA;
    state->Ds.Attributes = VMCB_ATTR_DATA;
    state->Es.Attributes = VMCB_ATTR_DATA;
    state->Fs.Attributes = VMCB_ATTR_DATA;
    state->Gs.Attributes = VMCB_ATTR_DATA;

    state->Cs.Base = 0;
    state->Ss.Base = 0;
    state->Ds.Base = 0;
    state->Es.Base = 0;

    state->Cs.Limit = 0xFFFFFFFF;
    state->Ss.Limit = 0xFFFFFFFF;
    state->Ds.Limit = 0xFFFFFFFF;
    state->Es.Limit = 0xFFFFFFFF;
    state->Fs.Limit = 0xFFFFFFFF;
    state->Gs.Limit = 0xFFFFFFFF;

    /*
        Keep host EFER during bring-up.
        Do not clear SVME until the VMRUN path is boring and stable.
    */
    state->Efer = AsmReadMsr64(MSR_EFER);

    state->Cr0 = AsmReadCr0();
    state->Cr3 = AsmReadCr3();
    state->Cr4 = AsmReadCr4();
    state->Cr2 = 0;

    state->Dr7 = 0x0000000000000400ULL;
    state->Dr6 = 0x00000000FFFF0FF0ULL;

    state->Cpl    = 0;
    state->Rflags = BIT1 | BIT9;
    state->Rip    = (UINT64)(UINTN)GuestBootTrampoline;
    state->Rsp    = guest_rsp;
    state->Rax    = 0;

    state->Star        = AsmReadMsr64(0xC0000081);
    state->Lstar        = AsmReadMsr64(0xC0000082);
    state->Cstar        = AsmReadMsr64(0xC0000083);
    state->Sfmask       = AsmReadMsr64(0xC0000084);
    state->KernelGsBase = AsmReadMsr64(0xC0000102);
    state->SysenterCs   = AsmReadMsr64(0x174);
    state->SysenterEsp  = AsmReadMsr64(0x175);
    state->SysenterEip  = AsmReadMsr64(0x176);
    state->Pat          = AsmReadMsr64(0x277);

    // KernelGsBase — SWAPGS points to null without this, instant crash
    state->KernelGsBase = AsmReadMsr64(0xC0000102);

    // PAT — zeroed PAT forces UC on all memory, kills display and perf
    state->Pat         = AsmReadMsr64(0x277);

    LOG(L"VMCB initialized");
    LOG(L"Guest RIP:  0x%016llx", state->Rip);
    LOG(L"Guest RSP:  0x%016llx", state->Rsp);
    LOG(L"Guest EFER: 0x%016llx", state->Efer);
    LOG(L"Guest CR0:  0x%016llx", state->Cr0);
    LOG(L"Guest CR3:  0x%016llx", state->Cr3);
    LOG(L"Guest CR4:  0x%016llx", state->Cr4);
    LOG(L"CS: %04x/%04x", state->Cs.Selector, state->Cs.Attributes);
    LOG(L"SS: %04x/%04x", state->Ss.Selector, state->Ss.Attributes);
    LOG(L"TR: %04x/%04x", state->Tr.Selector, state->Tr.Attributes);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
RunGuestRuntime(PAGES* pages, SVM_BOOT_MODE Mode)
{
    if (pages == NULL || pages->runtime_loop == 0 || pages->host_pml4 == 0 || pages->host_rsp == 0)
        return EFI_INVALID_PARAMETER;

    LOG(L"Switching host CR3: 0x%lx", pages->host_pml4);
    LOG(L"Deploying runtime VM loop: 0x%lx", pages->runtime_loop);

    (VOID)Mode;
    ((RUNTIME_VM_LOOP)(UINTN)pages->runtime_loop)(pages->vmcb_page, pages->hs_page, pages->host_pml4, pages->host_rsp);

    return EFI_DEVICE_ERROR;
}

EFI_STATUS
RunGuest(EFI_PHYSICAL_ADDRESS vmcb_pa, EFI_PHYSICAL_ADDRESS hsave_page, SVM_BOOT_MODE Mode)
{
    VMCB_CONTROL_AREA    *control = (VMCB_CONTROL_AREA   *)((UINTN)vmcb_pa + VMCB_CONTROL_OFFSET);
    VMCB_STATE_SAVE_AREA *state   = (VMCB_STATE_SAVE_AREA*)((UINTN)vmcb_pa + VMCB_STATE_OFFSET);

    GUEST_CONTEXT guest_ctx = { 0 };
    GUEST_CONTEXT *context = &guest_ctx;

    LOG(L"Pre-VMRUN: CS=%04x/%04x EFER=%llx CR0=%llx TR=%04x/%04x", state->Cs.Selector, state->Cs.Attributes, state->Efer, state->Cr0, state->Tr.Selector, state->Tr.Attributes);
    LOG(L"VMCB PA:        0x%llx", vmcb_pa);
    LOG(L"HSAVE PA:       0x%llx", hsave_page);
    LOG(L"ASID:           %u",     control->GuestAsid);
    LOG(L"InterceptMisc2: 0x%x",   control->InterceptMisc2);
    LOG(L"MsrpmBasePa:    0x%llx", control->MsrpmBasePa);
    LOG(L"ES:   %04x/%04x", state->Es.Selector,   state->Es.Attributes);
    LOG(L"CS:   %04x/%04x", state->Cs.Selector,   state->Cs.Attributes);
    LOG(L"SS:   %04x/%04x", state->Ss.Selector,   state->Ss.Attributes);
    LOG(L"DS:   %04x/%04x", state->Ds.Selector,   state->Ds.Attributes);
    LOG(L"LDTR: %04x/%04x", state->Ldtr.Selector, state->Ldtr.Attributes);
    LOG(L"TR:   %04x/%04x", state->Tr.Selector,   state->Tr.Attributes);
    LOG(L"DR6:  0x%llx DR7: 0x%llx", state->Dr6, state->Dr7);
    LOG(L"GDTR: base=0x%llx limit=0x%x", state->Gdtr.Base, state->Gdtr.Limit);
    LOG(L"IDTR: base=0x%llx limit=0x%x", state->Idtr.Base, state->Idtr.Limit);
    UINT64 *raw = (UINT64*)(UINTN)vmcb_pa;
    LOG(L"VMCB[0x00]: %016llx", raw[0]);   // InterceptCr/Dr/Exception
    LOG(L"VMCB[0x08]: %016llx", raw[1]);   // InterceptMisc1/2
    LOG(L"VMCB[0x10]: %016llx", raw[2]);   // InterceptMisc3/reserved
    LOG(L"VMCB[0x58]: %016llx", raw[11]);  // GuestAsid/TlbControl
    LOG(L"InterceptException: 0x%08x", control->InterceptException);
    LOG(L"InterceptMisc1:     0x%08x", control->InterceptMisc1);
    LOG(L"InterceptMisc2:     0x%08x", control->InterceptMisc2);
    LOG(L"GuestAsid:          %u",     control->GuestAsid);
    LOG(L"TlbControl:         0x%02x", control->TlbControl);
    LOG(L"Guest CR4: 0x%016llx", state->Cr4);
    LOG(L"Guest CR3: 0x%016llx", state->Cr3);
    LOG(L"Guest RIP: 0x%016llx", state->Rip);
    LOG(L"GDTR base full: 0x%016llx", state->Gdtr.Base);
    LOG(L"IDTR base full: 0x%016llx", state->Idtr.Base);
    LOG(L"Guest EFER full: 0x%016llx", state->Efer);
    LOG(L"Host  EFER full: 0x%016llx", AsmReadMsr64(MSR_EFER));
    LOG(L"CR0: 0x%016llx  CR3: 0x%016llx  CR4: 0x%016llx", state->Cr0, state->Cr3, state->Cr4);
    LOG(L"CS base: 0x%016llx  CS limit: 0x%08x", state->Cs.Base, state->Cs.Limit);
    LOG(L"SS base: 0x%016llx  SS limit: 0x%08x", state->Ss.Base, state->Ss.Limit);
    LOG(L"SS attr: 0x%04x", state->Ss.Attributes);

    while (TRUE) {
        // Pass the pointer directly, NOT the address of the pointer
        AsmVmRun(vmcb_pa, context);

        // Mirror hardware RAX into your context tracking structure
        context->Rax = state->Rax;

        if (control->ExitCode >= SVM_EXIT_EXCP_BASE &&
            control->ExitCode <= SVM_EXIT_EXCP_BASE + 0x1F) {
            if (gWindowsStarted) {
                RecordFatalExit(control, state);
                CpuDeadLoop();
            }
            ERROR(L"#VMEXIT: exception vector 0x%llx RIP=0x%llx info1=0x%llx info2=0x%llx",
                control->ExitCode - SVM_EXIT_EXCP_BASE,
                state->Rip,
                control->ExitInfo1,
                control->ExitInfo2);
            return EFI_UNSUPPORTED;
        }

        switch (control->ExitCode) {

            case SVM_EXIT_INIT:
                break;

            case SVM_EXIT_MSR:
                // control->ExitInfo1 == 1 indicates a WRMSR instruction
                // control->ExitInfo1 == 0 indicates a RDMSR instruction
                if (context->Rcx == 0xC0000080) { // MSR_EFER
                    if (control->ExitInfo1 == 1) {
                        UINT64 requested_efer = (context->Rdx << 32) | (context->Rax & 0xFFFFFFFF);
                        state->Efer = requested_efer | MSR_EFER_SVME; 
                    } 
                    else {
                        context->Rax = (UINT32)(state->Efer & 0xFFFFFFFF);
                        context->Rdx = (UINT32)(state->Efer >> 32);
                    }
                    // Synchronize RAX changes to VMCB
                    state->Rax = context->Rax;
                    state->Rip = control->NRip ? control->NRip : state->Rip + 2;
                } 
                else {
                    // --- PASS-THROUGH FALLBACK FOR WINDOWS KERNEL MSRs ---
                    if (control->ExitInfo1 == 1) { 
                        // Guest is executing WRMSR: Combine EDX:EAX and write to raw hardware
                        UINT64 val_to_write = (context->Rdx << 32) | (context->Rax & 0xFFFFFFFF);
                        AsmWriteMsr64((UINT32)context->Rcx, val_to_write);
                    } 
                    else { 
                        // Guest is executing RDMSR: Read raw hardware and split into EDX:EAX
                        UINT64 physical_msr_val = AsmReadMsr64((UINT32)context->Rcx);
                        context->Rax = (UINT32)(physical_msr_val & 0xFFFFFFFF);
                        context->Rdx = (UINT32)(physical_msr_val >> 32);
                        
                        // Synchronize updated RAX back to the hardware VMCB state area
                        state->Rax = context->Rax;
                    }

                    // Advance the guest past the 2-byte RDMSR/WRMSR instruction
                    state->Rip = control->NRip ? control->NRip : state->Rip + 2;
                }
                break;
            case SVM_EXIT_VMMCALL:
                if (!gWindowsStarted)
                    LOG(L"VMEXIT: VMCALL RAX=0x%11x", state->Rax);

                switch (state->Rax) {
                    case HV_CALL_GET_VERSION:
                        state->Rax = HV_VERSION;
                        if (!gWindowsStarted)
                            LOG(L"  -> GET_VERSION, returning 0x%llx", (UINT64)HV_VERSION);
                        break;

                    case HV_CALL_PING:
                        state->Rax = HV_PONG;
                        if (!gWindowsStarted)
                            LOG(L"  -> PING, returning 0x%llx", (UINT64)HV_PONG);
                        break;

                    default:
                        if (!gWindowsStarted)
                            LOG(L"  -> unknown hypercall 0x%llx", state->Rax);
                        state->Rax = (UINT64)-1;
                        break;
                }
                if (!gWindowsStarted)
                    LOG(L"RIP=0x%llx NRIP=0x%llx", state->Rip, control->NRip);
                state->Rip = control->NRip ? control->NRip : state->Rip + 3;
                break;

            case SVM_EXIT_HLT:
                if (Mode == SvmBootWindows) {
                    RecordFatalExit(control, state);
                    CpuDeadLoop();
                }
                LOG(L"#VMEXIT: HLT guest completed cleanly");
                return EFI_SUCCESS;

            case SVM_EXIT_INVALID:
                if (gWindowsStarted) {
                    RecordFatalExit(control, state);
                    CpuDeadLoop();
                }
                LOG(L"#VMEXIT: VMCB invalid state (check segment attrs / EFER)");
                return EFI_UNSUPPORTED;

            default:
                if (gWindowsStarted) {
                    RecordFatalExit(control, state);
                    CpuDeadLoop();
                }
                LOG(L"#VMEXIT: unhandled 0x%llx  RIP=0x%llx",
                    control->ExitCode, state->Rip);
                return EFI_UNSUPPORTED;
        }

        control->VmcbCleanBits = 0;
    }
}
