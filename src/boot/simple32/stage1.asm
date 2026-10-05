BITS 16
ORG 0x7C00

%ifndef STAGE2_LBA
%define STAGE2_LBA 2800
%endif

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 24
%endif

jmp short start
nop

; BIOS Parameter Block (standard 1.44MB FAT12)
OEMLabel            db 'SYS1BOOT'
BytesPerSector      dw 512
SectorsPerCluster   db 1
ReservedSectors     dw 1
NumberOfFATs        db 2
RootEntries         dw 224
TotalSectors16      dw 2880
Media               db 0xF0
SectorsPerFAT       dw 9
SectorsPerTrack     dw 18
NumberOfHeads       dw 2
HiddenSectors       dd 0
TotalSectors32      dd 0
DriveNumber         db 0
Reserved1           db 0
BootSignature       db 0x29
VolumeID            dd 0x53314D47
VolumeLabel         db 'SYSTEM1 FLOP'
FileSystemType      db 'FAT12   '

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov bx, 0x7E00
    mov ax, STAGE2_LBA
    mov cx, STAGE2_SECTORS

.load_loop:
    push ax
    push bx
    call read_sector_lba
    jc disk_error
    pop bx
    pop ax

    add bx, 512
    inc ax
    loop .load_loop

    jmp 0x0000:0x7E00

read_sector_lba:
    ; IN: AX=lba, BX=dest, DL=drive
    push ax
    push bx
    push cx
    push dx
    push si

    xor dx, dx
    mov cx, 18
    div cx              ; AX=temp, DX=sector_index
    mov cl, dl
    inc cl              ; sector number (1-18)

    xor dx, dx
    mov si, 2
    div si              ; AX=cylinder, DX=head

    mov ch, al          ; cylinder low 8
    mov dh, dl          ; head
    and cl, 0x3F

    mov dl, [boot_drive]
    mov ax, 0x0201
    int 0x13

    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

disk_error:
    mov si, msg_disk
    call print
    cli
.hang:
    hlt
    jmp .hang

print:
    mov ah, 0x0E
.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next
.done:
    ret

boot_drive db 0
msg_disk db 'Disk read error', 0

times 510-($-$$) db 0
dw 0xAA55
