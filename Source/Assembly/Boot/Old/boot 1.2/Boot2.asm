; ========================================================
; Kaon Stage 2
;
; 16-bit Real Mode
;       ->
; 32-bit Protected Mode
;       ->
; 64-bit Long Mode
;
; Stage 2 is loaded at physical address 5000h.
; ========================================================

bits 16
org 0x0000


; ========================================================
; CONSTANTS
; ========================================================

KAON_BASE       equ 0x5000

CODE32_SEG      equ 0x08
DATA_SEG        equ 0x10
CODE64_SEG      equ 0x18

PML4            equ 0x7000
PDPT            equ 0x8000
PAGE_DIR        equ 0x9000

STACK_TOP       equ 0xA000


; ========================================================
; 16-BIT REAL MODE
; ========================================================

Start:

        cli

        ; ------------------------------------------------
        ; Stage 2 is loaded at:
        ;
        ;       physical 5000h
        ;
        ; Bootloader enters with:
        ;
        ;       CS = 0500h
        ;       IP = 0000h
        ;
        ; 0500h:0000 = 5000h
        ; ------------------------------------------------

        mov ax, cs
        mov ds, ax
        mov es, ax

        xor ax, ax
        mov ss, ax
        mov sp, STACK_TOP


; ========================================================
; CHECK CPUID
; ========================================================

        pushfd
        pop eax

        mov ecx, eax

        xor eax, (1 << 21)

        push eax
        popfd

        pushfd
        pop eax

        xor eax, ecx

        ; If bit 21 could not be changed,
        ; CPUID is unavailable.

        jz NoCPUID

        ; Restore original EFLAGS.

        push ecx
        popfd


; ========================================================
; CHECK LONG MODE
; ========================================================

        mov eax, 0x80000000
        cpuid

        cmp eax, 0x80000001
        jb NoLongMode

        mov eax, 0x80000001
        cpuid

        ; EDX bit 29 = Long Mode support

        test edx, (1 << 29)
        jz NoLongMode


; ========================================================
; ENABLE A20
; ========================================================

        in al, 0x92

        ; Enable A20 while preserving other bits.

        or al, 00000010b

        out 0x92, al


; ========================================================
; LOAD GDT
; ========================================================

        lgdt [GDTDescriptor]


; ========================================================
; ENTER 32-BIT PROTECTED MODE
; ========================================================

        mov eax, cr0
        or eax, 1
        mov cr0, eax

        ; CODE32 descriptor has:
        ;
        ;       base = 5000h
        ;
        ; Therefore the offset ProtectedMode is relative
        ; to the beginning of Stage 2.

        jmp CODE32_SEG:ProtectedMode


; ========================================================
; 32-BIT PROTECTED MODE
; ========================================================

bits 32

ProtectedMode:

        ; ------------------------------------------------
        ; Load flat data segment.
        ; ------------------------------------------------

        mov ax, DATA_SEG

        mov ds, ax
        mov es, ax
        mov ss, ax

        mov esp, STACK_TOP

        cld


; ========================================================
; CLEAR PAGE TABLE MEMORY
; ========================================================

        ; Clear:
        ;
        ; 7000h - 9FFFh
        ;
        ; PML4
        ; PDPT
        ; PAGE DIRECTORY
        ;
        ; 3000h bytes = 3072 dwords.

        mov edi, PML4
        xor eax, eax
        mov ecx, 3072

        rep stosd


; ========================================================
; PML4
; ========================================================

        ; PML4[0] -> PDPT

        mov dword [PML4 + 0], PDPT | 0x03
        mov dword [PML4 + 4], 0


; ========================================================
; PDPT
; ========================================================

        ; PDPT[0] -> Page Directory

        mov dword [PDPT + 0], PAGE_DIR | 0x03
        mov dword [PDPT + 4], 0


; ========================================================
; PAGE DIRECTORY
;
; Identity-map the first 2 MiB using a 2 MiB page.
;
; Entry:
;
;       Present     = bit 0
;       Writable    = bit 1
;       Page Size   = bit 7
;
;       0x83
; ========================================================

        mov dword [PAGE_DIR + 0], 0x00000083
        mov dword [PAGE_DIR + 4], 0


; ========================================================
; LOAD CR3
; ========================================================

        mov eax, PML4
        mov cr3, eax


; ========================================================
; ENABLE PAE
; ========================================================

        mov eax, cr4
        or eax, (1 << 5)
        mov cr4, eax


; ========================================================
; ENABLE LONG MODE
;
; IA32_EFER MSR = C0000080h
;
; EFER bit 8 = LME
; ========================================================

        mov ecx, 0xC0000080

        rdmsr

        or eax, (1 << 8)

        wrmsr


; ========================================================
; ENABLE PAGING
;
; CR0 bit 31 = PG
; ========================================================

        mov eax, cr0
        or eax, (1 << 31)
        mov cr0, eax


; ========================================================
; ENTER 64-BIT LONG MODE
;
; The 64-bit code descriptor has BASE = 0.
;
; Unlike protected mode, the CS base is ignored in
; 64-bit mode.
;
; Therefore we jump to the REAL linear address:
;
;       KAON_BASE + LongMode
;
; ========================================================

        jmp CODE64_SEG:(KAON_BASE + LongMode)


; ========================================================
; 64-BIT LONG MODE
; ========================================================

bits 64

LongMode:

        cli

        ; ------------------------------------------------
        ; Load flat data segment.
        ; ------------------------------------------------

        mov ax, DATA_SEG

        mov ds, ax
        mov es, ax
        mov ss, ax

        mov rsp, STACK_TOP

        cld


; ========================================================
; PRINT SUCCESS MESSAGE
; ========================================================

        mov rdi, 0xB8000
        mov rsi, KAON_BASE + LongModeMessage


PrintLongMode:

        lodsb

        test al, al
        jz LongModeDone

        mov ah, 0x0F

        stosw

        jmp PrintLongMode


LongModeDone:

        cli


; ========================================================
; HALT
; ========================================================

LongModeHang:

        hlt
        jmp LongModeHang


; ========================================================
; REAL-MODE ERROR HANDLING
; ========================================================

bits 16


NoCPUID:

        mov si, CPUIDMessage
        jmp PrintError


NoLongMode:

        mov si, LongModeErrorMessage
        jmp PrintError


; ========================================================
; BIOS TEXT OUTPUT
; ========================================================

PrintError:

        lodsb

        test al, al
        jz ErrorHalt

        mov ah, 0x0E
        xor bh, bh

        int 0x10

        jmp PrintError


; ========================================================
; ERROR HALT
; ========================================================

ErrorHalt:

        cli


ErrorLoop:

        hlt
        jmp ErrorLoop


; ========================================================
; GDT
; ========================================================

align 8


GDT:

; --------------------------------------------------------
; 00h - NULL
; --------------------------------------------------------

        dq 0


; --------------------------------------------------------
; 08h - 32-BIT CODE
;
; Base  = 5000h
; Limit = 4 GiB
;
; Present
; Ring 0
; Code
; Readable
; 32-bit
; 4 KiB granularity
; --------------------------------------------------------

        dw 0xFFFF
        dw KAON_BASE
        db (KAON_BASE >> 16) & 0xFF

        db 10011010b

        db 11001111b

        db (KAON_BASE >> 24) & 0xFF


; --------------------------------------------------------
; 10h - DATA
;
; Base  = 0
; Limit = 4 GiB
;
; Present
; Ring 0
; Writable
; 32-bit
; 4 KiB granularity
; --------------------------------------------------------

        dw 0xFFFF
        dw 0x0000
        db 0x00

        db 10010010b

        db 11001111b

        db 0x00


; --------------------------------------------------------
; 18h - 64-BIT CODE
;
; Base = 0
;
; Present
; Ring 0
; Code
; Readable
; Long Mode = 1
; --------------------------------------------------------

        dq 0x00AF9A000000FFFF


GDT_End:


; ========================================================
; GDT DESCRIPTOR
; ========================================================

GDTDescriptor:

        ; Size

        dw GDT_End - GDT - 1

        ; Physical/linear address of GDT.
        ;
        ; GDT lives inside Stage 2, which begins at 5000h.

        dd KAON_BASE + GDT


; ========================================================
; MESSAGES
; ========================================================

CPUIDMessage:

        db "KAON: CPUID REQUIRED", 0


LongModeErrorMessage:

        db "KAON: LONG MODE UNAVAILABLE", 0


LongModeMessage:

        db "KAON 64-BIT LONG MODE OK", 0