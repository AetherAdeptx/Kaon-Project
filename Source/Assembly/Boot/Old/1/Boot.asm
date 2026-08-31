; ============================================================
; KAON BOOT
; IBM PC 5150 / 8088
; 360 KB 5.25" floppy
;
; BIOS loads this sector at 0000:7C00.
;
; Sector 1 = Kaon Boot
; Sector 2 = Kaon Stage 2 header
;
; ============================================================

bits 16
org 0x7C00

KAON_SEG equ 0x0500


start:
    cli

    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7C00

    ; BIOS boot drive
    mov [boot_drive], dl

    sti


; ------------------------------------------------------------
; Read sector 2 into 0500:0000
; ------------------------------------------------------------

    mov ax, KAON_SEG
    mov es, ax

    xor bx, bx

    mov ah, 02h        ; BIOS read sectors
    mov al, 01h        ; read 1 sector

    mov ch, 00h        ; cylinder 0
    mov cl, 02h        ; sector 2

    mov dh, 00h        ; head 0
    mov dl, [boot_drive]

    int 13h

    jc failure


; ------------------------------------------------------------
; Check for KAON2 signature
; ------------------------------------------------------------

    cmp byte [es:0000h], 'K'
    jne failure

    cmp byte [es:0001h], 'A'
    jne failure

    cmp byte [es:0002h], 'O'
    jne failure

    cmp byte [es:0003h], 'N'
    jne failure

    cmp byte [es:0004h], '2'
    jne failure


; ------------------------------------------------------------
; KAON2 FOUND
;
; For now we only load sector 2.
; Later this will load the rest of the Kaon environment.
; ------------------------------------------------------------

    cli

    jmp KAON_SEG:0000h


; ------------------------------------------------------------
; No valid Kaon environment
; ------------------------------------------------------------

failure:

    cli

halt:
    hlt
    jmp halt


; ------------------------------------------------------------
; Variables
; ------------------------------------------------------------

boot_drive:
    db 0


; ------------------------------------------------------------
; Boot signature
; ------------------------------------------------------------

times 510 - ($ - $$) db 0

dw 0xAA55