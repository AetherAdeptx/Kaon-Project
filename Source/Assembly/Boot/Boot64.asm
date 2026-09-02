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

; The BIOS loads the freestanding C kernel here before long mode is enabled.
; A BIOS extended read cannot cross a 64 KiB boundary, so the build limits the
; initial kernel image to 127 sectors. A later storage driver can remove this
; bootstrap limitation.
SYSTEM_START_ADDRESS     equ 0x10000

; Keep early boot allocations in the first 8 MiB. Librarian owns the next
; contiguous 256 MiB once the C kernel validates this handoff.
MEBIBYTE                 equ 0x100000
HUGE_PAGE_SIZE           equ 2 * MEBIBYTE
SMALL_PAGE_SIZE          equ 0x1000
BOOT_MEMORY_BASE         equ 0
BOOT_MEMORY_SIZE         equ 8 * MEBIBYTE
LIBRARIAN_MEMORY_BASE    equ BOOT_MEMORY_BASE + BOOT_MEMORY_SIZE
LIBRARIAN_MEMORY_SIZE    equ 256 * MEBIBYTE
BOOT_HUGE_PAGE_COUNT     equ BOOT_MEMORY_SIZE / HUGE_PAGE_SIZE
LIBRARIAN_SMALL_PAGES    equ LIBRARIAN_MEMORY_SIZE / SMALL_PAGE_SIZE
LIBRARIAN_PAGE_TABLES    equ LIBRARIAN_SMALL_PAGES / 512

; Keep the page tables below both the boot stack and the kernel.
PML4_ADDRESS             equ 0x1000
PDPT_ADDRESS             equ 0x2000
PAGE_DIRECTORY_ADDRESS   equ 0x3000
LIBRARIAN_TABLES_ADDRESS equ 0x100000

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
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP_ADDRESS
    mov [boot_drive], dl

    call initialize_serial

    ; Load the C kernel (LBA 1 onward) into physical address 0x10000.
    mov si, disk_address_packet
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

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

    ; The 128 small-page tables occupy 512 KiB at 1 MiB, within the
    ; reserved boot-memory range and away from the kernel and stack.
    mov edi, LIBRARIAN_TABLES_ADDRESS
    mov ecx, (LIBRARIAN_PAGE_TABLES * SMALL_PAGE_SIZE) / 4
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
    ; Map the 8 MiB boot range with four 2 MiB pages.
    ;
    ; Present  = bit 0
    ; Writable = bit 1
    ; PS       = bit 7
    ; ====================================================

    mov edi, PAGE_DIRECTORY_ADDRESS
    mov ecx, BOOT_HUGE_PAGE_COUNT
    mov eax, 0x00000083

.map_next_boot_page:
    mov dword [edi], eax
    add edi, 8
    add eax, HUGE_PAGE_SIZE
    loop .map_next_boot_page

    ; Point the following 128 directory entries at Librarian's page tables.
    mov ecx, LIBRARIAN_PAGE_TABLES
    mov eax, LIBRARIAN_TABLES_ADDRESS | 0x03

.map_next_librarian_table:
    mov dword [edi], eax
    add edi, 8
    add eax, SMALL_PAGE_SIZE
    loop .map_next_librarian_table

    ; Identity map 8-264 MiB with 65,536 standard 4 KiB pages.
    mov edi, LIBRARIAN_TABLES_ADDRESS
    mov ecx, LIBRARIAN_SMALL_PAGES
    mov eax, LIBRARIAN_MEMORY_BASE | 0x03

.map_next_librarian_page:
    mov dword [edi], eax
    add edi, 8
    add eax, SMALL_PAGE_SIZE
    dec ecx
    jnz .map_next_librarian_page


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


disk_error:
    DEBUG_CHAR 'E'
    cli
    hlt
    jmp disk_error


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
    ; HAND OFF TO THE 64-BIT SYSTEM
    ; ====================================================

    ; System V argument registers communicate the authoritative memory
    ; layout to kernel_main through the assembly entry stub.
    mov edi, BOOT_MEMORY_BASE
    mov esi, BOOT_MEMORY_SIZE
    mov edx, LIBRARIAN_MEMORY_BASE
    mov ecx, LIBRARIAN_MEMORY_SIZE

    mov rax, [rel system_entry]
    jmp rax


; The kernel is linked at this physical address.
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
; BIOS EXTENDED-READ PACKET
; ========================================================

align 4
disk_address_packet:
    db 0x10, 0
    dw KERNEL_SECTORS
    dw 0x0000
    dw SYSTEM_START_ADDRESS >> 4
    dq 1

boot_drive:
    db 0


; ========================================================
; PAD TO 512 BYTES
; ========================================================

times 510 - ($ - $$) db 0

dw 0xAA55
