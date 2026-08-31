;   Kaon Boot v1.1
;
;   You are free to use this code, but under the condition
;   that I assume no civil liability for your use of it.
;

bits 16                         ; Assemble as 16-bit x86
org 0x7C00                      ; BIOS loads boot sector here

KAON_SEG        equ 0x0500     ; Stage 2 segment
STAGE2_SECTORS  equ 4         ; Number of Stage 2 sectors


Start:
        cli                     ; Disable interrupts

        xor ax, ax              ; AX = 0000h
        mov ds, ax              ; DS = 0000h
        mov ss, ax              ; SS = 0000h
        mov sp, 0x8000          ; Stack starts at 8000h, grows downward

        mov [BootDrive], dl     ; Save BIOS boot drive

        sti                     ; Enable interrupts


        ; -----------------------------------------------
        ; Load Stage 2
        ;
        ; Stage 2 begins immediately after the boot sector.
        ;
        ; BIOS sector numbering starts at 1:
        ;
        ; Sector 1 = boot sector
        ; Sector 2 = first Stage 2 sector
        ;
        ; Destination:
        ;   0500:0000
        ;
        ; Physical address:
        ;   0x5000
        ; -----------------------------------------------

        mov ax, KAON_SEG
        mov es, ax              ; ES = 0500h

        xor bx, bx              ; ES:BX = 0500:0000

        mov ah, 0x02            ; BIOS: read sectors
        mov al, STAGE2_SECTORS  ; Number of sectors

        mov ch, 0x00            ; Cylinder 0
        mov cl, 0x02            ; Sector 2
        mov dh, 0x00            ; Head 0
        mov dl, [BootDrive]     ; Boot drive

        int 0x13                ; BIOS disk read

        jc DiskError            ; CF set = disk read failed


        ; -----------------------------------------------
        ; Stage 2 loaded successfully
        ; -----------------------------------------------

        jmp KAON_SEG:0000


; -------------------------------------------------------
; Disk error
; -------------------------------------------------------

DiskError:
        mov si, DiskErrorMsg


PrintError:
        lodsb                   ; AL = next character

        cmp al, 0               ; End of string?
        je Halt

        mov ah, 0x0E            ; BIOS teletype output
        mov bh, 0x00            ; Display page 0
        int 0x10

        jmp PrintError


; -------------------------------------------------------
; Halt
; -------------------------------------------------------

Halt:
        cli

.hang:
        hlt
        jmp .hang


; -------------------------------------------------------
; Data
; -------------------------------------------------------

BootDrive       db 0

DiskErrorMsg    db "KAON: Stage 2 load failed", 0


; -------------------------------------------------------
; Boot sector padding
; -------------------------------------------------------

times 510-($-$$) db 0

dw 0xAA55