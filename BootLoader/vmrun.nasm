BITS 64

section .text

global AsmVmRun

; Windows x64 ABI / UEFI:
; RCX = VmcbPa
; RDX = Context pointer (passed from your updated C loop)

AsmVmRun:
    pushfq

    push rax
    push rbx
    push rcx            ; [rsp + 96] = Host RCX (VmcbPa)
    push rdx            ; [rsp + 88] = Host RDX (Context pointer)
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; -------------------------------------------------------------------------
    ; 1. Load Guest GPRs from the Context structure before entering the guest
    ; -------------------------------------------------------------------------
    mov rax, [rsp + 88] ; Temporarily read the Context pointer into RAX

    mov rbx, [rax + 24] ; Offset of Rbx
    mov rcx, [rax + 8]  ; Offset of Rcx
    mov rdx, [rax + 16] ; Offset of Rdx
    mov rbp, [rax + 40] ; Offset of Rbp
    mov rsi, [rax + 48] ; Offset of Rsi
    mov rdi, [rax + 56] ; Offset of Rdi
    mov r8,  [rax + 64] ; Offset of R8
    mov r9,  [rax + 72] ; Offset of R9
    mov r10, [rax + 80] ; Offset of R10
    mov r11, [rax + 88] ; Offset of R11
    mov r12, [rax + 96] ; Offset of R12
    mov r13, [rax + 104]; Offset of R13
    mov r14, [rax + 112]; Offset of R14
    mov r15, [rax + 120]; Offset of R15

    ; -------------------------------------------------------------------------
    ; 2. Setup VMCB Pointer and Run Guest
    ; -------------------------------------------------------------------------
    mov rax, [rsp + 96] ; VMRUN/VMLOAD/VMSAVE require RAX = VMCB physical address
                        ; (We fetch this from the host stack slot)

    clgi
    vmload              ; NASM form: no operand
    vmrun               ; NASM form: no operand
    vmsave              ; NASM form: no operand
    stgi

    ; =========================================================================
    ; --- A VM-EXIT OCCURS HERE ---
    ; Physical hardware registers now hold live guest execution values.
    ; =========================================================================

    ; -------------------------------------------------------------------------
    ; 3. Save Guest GPRs into the Context structure
    ; -------------------------------------------------------------------------
    ; Note: We can safely overwrite RAX because AMD CPUs automatically save 
    ; the guest's RAX register directly into the VMCB State-Save Area on VM-Exit.
    mov rax, [rsp + 88] ; Recover our Context pointer from the host stack slot

    mov [rax + 8],   rcx ; Save Guest RCX (Crucial for MSR ID checking!)
    mov [rax + 16],  rdx ; Save Guest RDX (Crucial for MSR value checking!)
    mov [rax + 24],  rbx ; Save Guest RBX
    mov [rax + 40],  rbp ; Save Guest RBP
    mov [rax + 48],  rsi ; Save Guest RSI
    mov [rax + 56],  rdi ; Save Guest RDI
    mov [rax + 64],  r8  ; Save Guest R8
    mov [rax + 72],  r9  ; Save Guest R9
    mov [rax + 80],  r10 ; Save Guest R10
    mov [rax + 88],  r11 ; Save Guest R11
    mov [rax + 96],  r12 ; Save Guest R12
    mov [rax + 104], r13 ; Save Guest R13
    mov [rax + 112], r14 ; Save Guest R14
    mov [rax + 120], r15 ; Save Guest R15

    ; -------------------------------------------------------------------------
    ; 4. Restore Original Host State and Return to C
    ; -------------------------------------------------------------------------
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    popfq
    ret