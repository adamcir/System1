BITS 16
ORG 0x7E00

%define KERNEL_LOAD_SEG 0x1000
%define BOOT_INFO_SEG   0x9000
%define FLOPPY_MAGIC    0x53314D47

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00
    sti

    mov [boot_drive], dl

    ; Read boot sector BPB
    mov ax, 0
    mov bx, boot_sector
    call read_lba_sector
    jc disk_fail

    mov ax, [boot_sector + 11]  ; bytes per sector
    cmp ax, 512
    jne bad_fs

    mov ax, [boot_sector + 22]  ; sectors per FAT
    mov [sectors_per_fat], ax

    mov al, [boot_sector + 16]  ; number of FATs
    xor ah, ah
    mov [num_fats], ax

    mov ax, [boot_sector + 17]  ; root entries
    mov [root_entries], ax

    ; root_dir_sectors = (root_entries*32 + 511) / 512
    mov ax, [root_entries]
    shl ax, 5
    add ax, 511
    mov bx, 512
    xor dx, dx
    div bx
    mov [root_dir_sectors], ax

    mov ax, [boot_sector + 14] ; reserved sectors
    mov [fat_lba], ax

    ; root_lba = fat_lba + num_fats * sectors_per_fat
    mov ax, [num_fats]
    mul word [sectors_per_fat]
    add ax, [fat_lba]
    mov [root_lba], ax

    ; data_lba = root_lba + root_dir_sectors
    mov ax, [root_lba]
    add ax, [root_dir_sectors]
    mov [data_lba], ax

    ; Read FAT #1 into memory
    mov di, fat_buffer
    mov ax, [fat_lba]
    mov cx, [sectors_per_fat]
.read_fat:
    push ax
    push cx
    mov bx, di
    call read_lba_sector
    jc disk_fail
    add di, 512
    pop cx
    pop ax
    inc ax
    loop .read_fat

    ; Search BOOT directory in root dir
    mov ax, [root_lba]
    mov cx, [root_dir_sectors]
    mov di, root_buffer
.read_root:
    push ax
    push cx
    mov bx, di
    call read_lba_sector
    jc disk_fail

    mov si, di
    mov dx, 16            ; 16 entries per sector
.scan_root_entries:
    cmp byte [si], 0x00
    je kernel_not_found
    cmp byte [si], 0xE5
    je .next_root_entry

    push si
    mov di, boot_dir_name
    mov cx, 11
    repe cmpsb
    pop si
    je boot_dir_found

.next_root_entry:
    add si, 32
    dec dx
    jnz .scan_root_entries

    pop cx
    pop ax
    inc ax
    add di, 512
    loop .read_root
    jmp kernel_not_found

boot_dir_found:
    pop cx  ; clean up stack
    pop ax  ; clean up stack

    ; Get cluster of BOOT directory
    mov ax, [si + 26]

    ; lba = data_lba + cluster - 2
    sub ax, 2
    add ax, [data_lba]

    ; Load the BOOT directory sector into root_buffer
    mov bx, root_buffer
    call read_lba_sector
    jc disk_fail

    ; Search KERNEL in BOOT directory sector
    mov si, root_buffer
    mov dx, 16            ; 16 entries in 512-byte sector
.scan_boot_entries:
    cmp byte [si], 0x00
    je kernel_not_found
    cmp byte [si], 0xE5
    je .next_boot_entry

    push si
    mov di, kernel_name
    mov cx, 11
    repe cmpsb
    pop si
    je kernel_found

.next_boot_entry:
    add si, 32
    dec dx
    jnz .scan_boot_entries
    jmp kernel_not_found

kernel_found:
    ; SI points to dir entry
    mov ax, [si + 26]
    mov [current_cluster], ax

    mov ax, [si + 28]
    mov [kernel_size], ax
    mov ax, [si + 30]
    mov [kernel_size + 2], ax

    mov ax, KERNEL_LOAD_SEG
    mov [load_segment], ax

.load_clusters:
    mov ax, [current_cluster]
    cmp ax, 0xFF8
    jae done_loading

    ; lba = data_lba + cluster - 2
    sub ax, 2
    add ax, [data_lba]

    mov bx, 0
    mov es, [load_segment]
    call read_lba_sector_esbx
    jc disk_fail

    ; next write location += 512 bytes => segment + 0x20
    mov ax, [load_segment]
    add ax, 0x20
    mov [load_segment], ax

    ; next FAT12 cluster
    mov ax, [current_cluster]
    call fat12_next_cluster
    mov [current_cluster], ax
    jmp .load_clusters

done_loading:
    call floppy_motor_off

    ; Enter Unreal Mode to load the entire floppy disk to 0x00200000 (2 MB)
    call enter_unreal

    ; Loop through LBA 0 to 2879
    xor cx, cx              ; CX = current_lba = 0
.load_floppy_loop:
    push cx
    mov ax, cx
    mov bx, boot_sector
    call read_lba_sector
    pop cx
    jnc .copy_sector
    jmp disk_fail

.copy_sector:
    ; Copy 512 bytes (128 dwords) from boot_sector to GS:[0x00200000 + CX * 512]
    push cx
    
    ; EDI = 0x00200000 + CX * 512
    xor edi, edi
    mov di, cx
    shl edi, 9
    add edi, 0x00200000

    mov ecx, 128
    mov esi, boot_sector
.copy_dword:
    mov eax, [esi]
    mov [gs:edi], eax
    add esi, 4
    add edi, 4
    dec ecx
    jnz .copy_dword

    pop cx
    inc cx
    cmp cx, 2880
    jne .load_floppy_loop

    ; Fill boot_info at BOOT_INFO_SEG:0
    mov ax, BOOT_INFO_SEG
    mov es, ax
    xor di, di

    xor eax, eax
    mov al, [boot_drive]
    stosd

    mov eax, 0x00010000
    stosd

    mov eax, [kernel_size]
    stosd

    mov eax, boot_sector
    stosd

    mov eax, fat_buffer
    stosd

    mov eax, root_buffer
    stosd

    xor eax, eax
    mov ax, [fat_lba]
    stosd

    xor eax, eax
    mov ax, [sectors_per_fat]
    stosd

    xor eax, eax
    mov ax, [root_lba]
    stosd

    xor eax, eax
    mov ax, [root_dir_sectors]
    stosd

    ; Member 11: floppy_image_addr
    mov eax, 0x00200000
    stosd

    call enter_protected_mode
    jmp $

kernel_not_found:
    mov si, msg_not_found
    call print
    jmp halt

bad_fs:
    mov si, msg_badfs
    call print
    jmp halt

disk_fail:
    mov si, msg_disk
    call print

halt:
    call floppy_motor_off
    cli
.hang:
    hlt
    jmp .hang

floppy_motor_off:
    ; DOR (0x3F2): keep controller enabled, all motor bits off.
    mov dx, 0x03F2
    mov al, 0x0C
    out dx, al
    ret

; AX = cluster
fat12_next_cluster:
    push bx
    push dx

    mov bx, ax
    mov dx, ax
    shr dx, 1
    add bx, dx              ; offset = n + n/2

    mov dx, [fat_buffer + bx]

    test ax, 1
    jz .even
    shr dx, 4
    and dx, 0x0FFF
    mov ax, dx
    jmp .done
.even:
    and dx, 0x0FFF
    mov ax, dx
.done:
    pop dx
    pop bx
    ret

; AX=lba, BX=offset in DS
read_lba_sector:
    push es
    xor dx, dx
    mov es, dx
    call read_lba_sector_esbx
    pop es
    ret

; AX=lba, ES:BX=dest
read_lba_sector_esbx:
    push ax
    push bx
    push cx
    push dx
    push si

    xor dx, dx
    mov cx, 18
    div cx                  ; AX=temp, DX=sector_index
    mov cl, dl
    inc cl                  ; sector 1..18

    xor dx, dx
    mov si, 2
    div si                  ; AX=cylinder, DX=head

    mov ch, al
    mov dh, dl
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

enter_unreal:
    push ds
    push es
    cli
    lgdt [gdt_ptr]

    ; Switch to protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Load flat 4GB selector into GS
    mov ax, 0x10
    mov gs, ax

    ; Switch back to real mode
    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax

    pop es
    pop ds
    sti
    ret

enter_protected_mode:
    in al, 0x92
    or al, 0x02
    out 0x92, al

    cli
    lgdt [gdt_ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode

BITS 32
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9FC00

    mov eax, FLOPPY_MAGIC
    mov ebx, 0x00090000
    jmp 0x00010000

BITS 16
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

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive       db 0
sectors_per_fat  dw 0
num_fats         dw 0
root_entries     dw 0
root_dir_sectors dw 0
fat_lba          dw 0
root_lba         dw 0
data_lba         dw 0
current_cluster  dw 0
load_segment     dw 0
kernel_size      dd 0

msg_not_found db 'boot/kernel.bin not found', 0
msg_badfs     db 'FAT12 expected', 0
msg_disk      db 'Disk read failure', 0
kernel_name   db 'KERNEL  BIN'
boot_dir_name db 'BOOT       '

align 2
boot_sector: times 512 db 0
fat_buffer:  times 4608 db 0
root_buffer: times 7168 db 0
