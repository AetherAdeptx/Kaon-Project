bits 16
org 0x7C00

; ========================================================
; PHYSICAL MEMORY LAYOUT
; ========================================================

STACK_BOTTOM_ADDRESS     equ 0x7000
STACK_TOP_ADDRESS        equ 0x7C00

BOOTLOADER_START_ADDRESS equ 0x7C00
BOOTLOADER_SIZE          equ 512
BOOTLOADER_END_ADDRESS   equ BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE

; Start at 1 MiB to avoid VGA, BIOS, and ROM address ranges in low memory.
SYSTEM_BUFFER_ADDRESS    equ 0x100000
SYSTEM_BUFFER_SIZE       equ 0x200000
SYSTEM_START_ADDRESS     equ SYSTEM_BUFFER_ADDRESS + SYSTEM_BUFFER_SIZE

; Keep the page tables below the stack and outside the system buffer.
PML4_ADDRESS             equ 0x1000
PDPT_ADDRESS             equ 0x2000
PAGE_DIRECTORY_ADDRESS   equ 0x3000

SERIAL_PORT              equ 0x3F8

; Mirror each debugging character to QEMU's debug port and COM1.
%macro DEBUG_CHAR 1
    mov al, %1
    out 0xE9, al
    mov dx, SERIAL_PORT
    out dx, al
%endmacro

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP_ADDRESS

    call initialize_serial

    ; ====================================================
    ; REAL MODE CHECK
    ; ====================================================

    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x0F52       ; R

    DEBUG_CHAR 'R'

    xor ax, ax
    mov ds, ax
    mov es, ax


    ; ====================================================
    ; ENABLE A20
    ; ====================================================

    in al, 0x92
    or al, 2
    out 0x92, al


    ; ====================================================
    ; LOAD GDT
    ; ====================================================

    lgdt [gdt_descriptor]


    ; ====================================================
    ; CLEAR PAGE TABLE AREA
    ;
    ; 1000 = PML4
    ; 2000 = PDPT
    ; 3000 = PAGE DIRECTORY
    ; ====================================================

    xor eax, eax
    mov edi, PML4_ADDRESS
    mov ecx, (3 * 4096) / 4
    rep stosd


    ; ====================================================
    ; PML4 -> PDPT
    ; ====================================================

    mov dword [PML4_ADDRESS], PDPT_ADDRESS | 0x03


    ; ====================================================
    ; PDPT -> PAGE DIRECTORY
    ; ====================================================

    mov dword [PDPT_ADDRESS], PAGE_DIRECTORY_ADDRESS | 0x03


    ; ====================================================
    ; PAGE DIRECTORY
    ;
    ; Identity map first 4 MiB using two 2 MiB pages. This
    ; includes the system entry point immediately after the
    ; 2 MiB loading buffer.
    ;
    ; Present  = bit 0
    ; Writable = bit 1
    ; PS       = bit 7
    ; ====================================================

    mov dword [PAGE_DIRECTORY_ADDRESS],     0x00000083
    mov dword [PAGE_DIRECTORY_ADDRESS + 8], 0x00200083


    ; ====================================================
    ; LOAD CR3
    ; ====================================================

    mov eax, PML4_ADDRESS
    mov cr3, eax


    ; ====================================================
    ; ENABLE PAE
    ; ====================================================

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax


    ; ====================================================
    ; ENABLE LONG MODE
    ; ====================================================

    mov ecx, 0xC0000080
    rdmsr

    or eax, (1 << 8)

    wrmsr


    ; ====================================================
    ; ENTER PROTECTED MODE
    ; ====================================================

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected


; ========================================================
; INITIALIZE COM1 SERIAL (115200 BAUD, 8N1)
; ========================================================

initialize_serial:
    mov dx, SERIAL_PORT + 1
    xor al, al                    ; Disable interrupts
    out dx, al

    mov dx, SERIAL_PORT + 3
    mov al, 0x80                  ; Enable divisor latch
    out dx, al

    mov dx, SERIAL_PORT
    mov al, 0x01                  ; Divisor 1 = 115200 baud
    out dx, al

    mov dx, SERIAL_PORT + 1
    xor al, al
    out dx, al

    mov dx, SERIAL_PORT + 3
    mov al, 0x03                  ; 8 data bits, no parity, 1 stop bit
    out dx, al

    mov dx, SERIAL_PORT + 2
    mov al, 0xC7                  ; Enable and clear FIFOs
    out dx, al

    mov dx, SERIAL_PORT + 4
    mov al, 0x0B                  ; Enable IRQs, RTS, and DTR
    out dx, al
    ret


; ========================================================
; 32-BIT PROTECTED MODE
; ========================================================

bits 32

protected:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; ====================================================
    ; PROTECTED MODE CHECK
    ; ====================================================

    mov dword [0xB8002], 0x0F500F50

    DEBUG_CHAR 'P'


    ; ====================================================
    ; ENABLE PAGING
    ; ====================================================

    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax


    ; ====================================================
    ; ENTER 64-BIT LONG MODE
    ; ====================================================

    jmp 0x18:long_mode


; ========================================================
; 64-BIT LONG MODE
; ========================================================

bits 64

long_mode:

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rsp, STACK_TOP_ADDRESS

    ; ====================================================
    ; LONG MODE CHECK
    ; ====================================================

    mov word [0xB8004], 0x0F4C       ; L

    DEBUG_CHAR 'L'


    ; ====================================================
    ; PRINT KAON
    ; ====================================================

    mov rdi, 0xB8000

    mov word [rdi + 10], 0x0F4B     ; K
    mov word [rdi + 12], 0x0F41     ; A
    mov word [rdi + 14], 0x0F4F     ; O
    mov word [rdi + 16], 0x0F4E     ; N

    DEBUG_CHAR 'K'
    DEBUG_CHAR 'A'
    DEBUG_CHAR 'O'
    DEBUG_CHAR 'N'
    DEBUG_CHAR 13
    DEBUG_CHAR 10

    ; ====================================================
    ; HAND OFF TO THE 64-BIT SYSTEM
    ; ====================================================

    mov rax, [rel system_entry]
    jmp rax


; Runtime-configurable entry point immediately after the 2 MiB buffer.
system_entry:
    dq SYSTEM_START_ADDRESS


hang:
    cli
    hlt
    jmp hang


; ========================================================
; GDT
; ========================================================

bits 16

align 8

gdt:

    ; Null
    dq 0x0000000000000000

    ; 08h - 32-bit code
    dq 0x00CF9A000000FFFF

    ; 10h - data
    dq 0x00CF92000000FFFF

    ; 18h - 64-bit code
    dq 0x00AF9A000000FFFF

gdt_end:


; ========================================================
; GDT DESCRIPTOR
; ========================================================

gdt_descriptor:
    dw gdt_end - gdt - 1
    dd gdt


; ========================================================
; PAD TO 512 BYTES
; ========================================================

times 510 - ($ - $$) db 0

dw 0xAA55
