bits 64
default rel

section .text
global kernel_entry
extern kernel_main
extern __bss_start
extern __bss_end

kernel_entry:
    ; Preserve the bootloader's four memory-layout arguments while clearing
    ; BSS, then forward them to the C kernel using the System V AMD64 ABI.
    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    ; The System V AMD64 ABI expects a 16-byte-aligned stack before a call.
    mov rsp, 0x90000
    and rsp, -16

    ; A flat kernel image does not contain the NOBITS .bss payload.
    cld
    lea rdi, [__bss_start]
    lea rcx, [__bss_end]
    sub rcx, rdi
    xor eax, eax
    rep stosb

    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    mov rcx, r15
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
