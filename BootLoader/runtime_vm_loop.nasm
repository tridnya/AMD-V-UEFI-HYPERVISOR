BITS 64

section .text

global RuntimeVmLoopTemplate
global RuntimeVmLoopTemplateEnd

%define SVM_EXIT_INIT     072h
%define SVM_EXIT_MSR      07Ch
%define SVM_EXIT_VMMCALL  081h

%define HV_GET_VERSION    1
%define HV_PING           2
%define HV_VERSION        010000h
%define HV_PONG           0DEADBEEFh

%define MSR_EFER          0C0000080h
%define EFER_SVME         1000h

; VMCB control area offsets
%define VMCB_EXITCODE     070h
%define VMCB_EXITINFO1    078h
%define VMCB_CLEAN_BITS   0C0h
%define VMCB_NRIP         0C8h

; VMCB state save area offsets
%define VMCB_EFER         4D0h
%define VMCB_RIP          578h
%define VMCB_RAX          5F8h

; Host stack frame layout
%define FRAME_VMCB        000h
%define FRAME_HAS_GPRS    008h
%define FRAME_LIVE_RAX    010h
%define FRAME_RBX         018h
%define FRAME_RCX         020h
%define FRAME_RDX         028h
%define FRAME_RBP         030h
%define FRAME_RSI         038h
%define FRAME_RDI         040h
%define FRAME_R8          048h
%define FRAME_R9          050h
%define FRAME_R10         058h
%define FRAME_R11         060h
%define FRAME_R12         068h
%define FRAME_R13         070h
%define FRAME_R14         078h
%define FRAME_R15         080h
%define FRAME_SIZE        100h

; Windows x64 ABI:
; RCX = VmcbPa
; RDX = HsavePage, already installed in MSR_VM_HSAVE_PA
; R8  = HostPml4
; R9  = HostRsp
RuntimeVmLoopTemplate:
    mov     rsp, r9
    and     rsp, 0FFFFFFFFFFFFFFF0h
    sub     rsp, FRAME_SIZE

    mov     [rsp + FRAME_VMCB], rcx

    ; Switch host-side runtime loop to identity-mapped host page tables.
    mov     rax, r8
    mov     cr3, rax

    xor     eax, eax
    mov     [rsp + FRAME_HAS_GPRS], rax

.run:
    cmp     qword [rsp + FRAME_HAS_GPRS], 0
    je      .enter_guest

    ; Restore guest GPRs before re-entering.
    ; VMRUN loads guest RAX from VMCB_RAX automatically.
    mov     rbx, [rsp + FRAME_RBX]
    mov     rcx, [rsp + FRAME_RCX]
    mov     rdx, [rsp + FRAME_RDX]
    mov     rbp, [rsp + FRAME_RBP]
    mov     rsi, [rsp + FRAME_RSI]
    mov     rdi, [rsp + FRAME_RDI]
    mov     r8,  [rsp + FRAME_R8]
    mov     r9,  [rsp + FRAME_R9]
    mov     r10, [rsp + FRAME_R10]
    mov     r11, [rsp + FRAME_R11]
    mov     r12, [rsp + FRAME_R12]
    mov     r13, [rsp + FRAME_R13]
    mov     r14, [rsp + FRAME_R14]
    mov     r15, [rsp + FRAME_R15]

.enter_guest:
    ; VMRUN requires RAX = VMCB physical address.
    mov     rax, [rsp + FRAME_VMCB]
    clgi
    vmrun

    ; Save all live GPRs immediately after VMEXIT.
    mov     [rsp + FRAME_LIVE_RAX], rax
    mov     [rsp + FRAME_RBX], rbx
    mov     [rsp + FRAME_RCX], rcx
    mov     [rsp + FRAME_RDX], rdx
    mov     [rsp + FRAME_RBP], rbp
    mov     [rsp + FRAME_RSI], rsi
    mov     [rsp + FRAME_RDI], rdi
    mov     [rsp + FRAME_R8],  r8
    mov     [rsp + FRAME_R9],  r9
    mov     [rsp + FRAME_R10], r10
    mov     [rsp + FRAME_R11], r11
    mov     [rsp + FRAME_R12], r12
    mov     [rsp + FRAME_R13], r13
    mov     [rsp + FRAME_R14], r14
    mov     [rsp + FRAME_R15], r15

    mov     rax, 1
    mov     [rsp + FRAME_HAS_GPRS], rax

    mov     rbx, [rsp + FRAME_VMCB]
    mov     rax, [rbx + VMCB_EXITCODE]

    cmp     rax, SVM_EXIT_VMMCALL
    je      .vmmcall

    cmp     rax, SVM_EXIT_INIT
    je      .advance_rip

    cmp     rax, SVM_EXIT_MSR
    je      .msr_exit

    jmp     .fatal

.vmmcall:
    mov     rax, [rbx + VMCB_RAX]
    cmp     eax, HV_GET_VERSION
    je      .get_version
    cmp     eax, HV_PING
    je      .ping
    jmp     .unknown

.get_version:
    mov     rax, HV_VERSION
    mov     [rbx + VMCB_RAX], rax
    jmp     .advance_rip

.ping:
    mov     rax, HV_PONG
    mov     [rbx + VMCB_RAX], rax
    jmp     .advance_rip

.unknown:
    ; Packed debug return:
    ; bits 0..15 VMCB_RAX, 16..31 LIVE_RAX, 32..47 RCX, 48..63 R10.
    xor     rax, rax

    mov     rdx, [rbx + VMCB_RAX]
    and     rdx, 0FFFFh
    or      rax, rdx

    mov     rdx, [rsp + FRAME_LIVE_RAX]
    and     rdx, 0FFFFh
    shl     rdx, 16
    or      rax, rdx

    mov     rdx, [rsp + FRAME_RCX]
    and     rdx, 0FFFFh
    shl     rdx, 32
    or      rax, rdx

    mov     rdx, [rsp + FRAME_R10]
    and     rdx, 0FFFFh
    shl     rdx, 48
    or      rax, rdx

    mov     [rbx + VMCB_RAX], rax
    jmp     .advance_rip

.msr_exit:
    ; ExitInfo1 = 0 means read, 1 means write.
    mov     rax, [rbx + VMCB_EXITINFO1]
    test    rax, rax
    jz      .advance_msr_rip

    ; Guest RCX holds the MSR number.
    mov     rax, [rsp + FRAME_RCX]
    cmp     eax, MSR_EFER
    jne     .advance_msr_rip

    ; WRMSR value is guest RDX:RAX. RAX lives in VMCB, RDX in saved GPRs.
    mov     r10d, [rbx + VMCB_RAX]
    mov     r11d, [rsp + FRAME_RDX]
    or      r10d, EFER_SVME

    shl     r11, 32
    or      r11, r10
    mov     [rbx + VMCB_EFER], r11

    jmp     .advance_msr_rip

.advance_msr_rip:
    mov     rax, [rbx + VMCB_NRIP]
    test    rax, rax
    jnz     .store_rip

    mov     rax, [rbx + VMCB_RIP]
    add     rax, 2

    jmp     .store_rip

.advance_rip:
    mov     rax, [rbx + VMCB_NRIP]
    test    rax, rax
    jnz     .store_rip

    mov     rax, [rbx + VMCB_RIP]
    add     rax, 3

.store_rip:
    mov     [rbx + VMCB_RIP], rax
    mov     dword [rbx + VMCB_CLEAN_BITS], 0
    jmp     .run

.fatal:
    mov     [rbx + VMCB_RAX], rax
    pause
    jmp     .fatal

RuntimeVmLoopTemplateEnd:
