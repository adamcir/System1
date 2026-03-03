# System/1 Plan: 3 Kernel Branches (`i386`, `x86_64`, `i386-floppy`)

## Summary
Projekt bude mít tři oddělené kernel větve:

1. `kernel/i386/` pro GRUB `iso-32`
2. `kernel/x86_64/` pro GRUB `iso-x86_64`
3. `kernel/i386-floppy/` pro jednoduchý bootloader `img-32`

Každá větev má vlastní `kernel.c` a vlastní `modules/`.

## Target Layout
1. `kernel/i386/kernel.c`
2. `kernel/i386/modules/{vga,panic,bootlog}/...`
3. `kernel/x86_64/kernel.c`
4. `kernel/x86_64/modules/{vga,panic,bootlog}/...`
5. `kernel/i386-floppy/kernel.c`
6. `kernel/i386-floppy/modules/{vga,panic,bootlog}/...`

## Boot Pipelines
1. `iso-32`: `boot/multiboot2_header.S` + `arch/i386/entry_i386.S` + `kernel/i386/*`
2. `iso-x86_64`: `boot/multiboot2_header.S` + `arch/x86_64/entry_x86_64_bridge.S` + `kernel/x86_64/*`
3. `img-32`: `boot/simple32/stage1.asm` + `boot/simple32/stage2.asm` + `arch/i386/entry_floppy_i386.S` + `kernel/i386-floppy/*`

## GRUB Layout
1. `boot/grub/i386/grub.cfg` pro `iso-32`
2. `boot/grub/x86_64/grub.cfg` pro `iso-x86_64`
3. ISO skripty berou `grub.cfg` jen z `boot/grub/*` (ne ze staré `grub/` složky)

## Public APIs / Interfaces
1. `kernel/i386/kernel.c`:
   - `void kmain_i386(uint32_t magic, uint32_t info);`
2. `kernel/x86_64/kernel.c`:
   - `void kmain_x86_64(uint32_t magic, uint32_t info);`
3. `kernel/i386-floppy/kernel.c`:
   - `void kmain_floppy_i386(uint32_t magic, uint32_t boot_info_ptr);`
4. Module API (v každé větvi stejné názvy):
   - `vga_init`, `vga_putc`, `vga_puts`
   - `panic`
   - `bootlog_info`

## Build System Changes (Makefile)
1. Oddělené proměnné:
   - `KDIR_I386 := kernel/i386`
   - `KDIR_X64 := kernel/x86_64`
   - `KDIR_FLP := kernel/i386-floppy`
2. Každý target kompiluje jen vlastní `kernel.c` + vlastní `modules/*`.
3. Moduly se archivují do `.a` odděleně po větvích.
4. Výstupy:
   - `build/system1-iso-32.iso`
   - `build/system1-iso-x86_64.iso`
   - `build/system1-img-32.img`
5. Cíle:
   - `iso-32`, `iso-x86_64`, `img-32`
   - `run-iso-32`, `run-iso-x86_64`, `run-img-32`
   - `modules-i386`, `modules-x86_64`, `modules-i386-floppy` (doporučeno)

## `img-32` (Floppy) Details
1. Stage1 načte stage2.
2. Stage2 čte FAT12 root directory a hledá `KERNEL32.BIN`.
3. Kernel načte do `0x00100000`, přepne do protected mode a skočí na `entry_floppy_i386`.
4. `kmain_floppy_i386` vypíše boot status přes `bootlog`/`vga`.

## Test Cases
1. `make clean && make iso-32` projde a bootuje.
2. `make clean && make iso-x86_64` projde a bootuje.
3. `make clean && make img-32` vytvoří 1.44MB image a bootuje.
4. Ověření izolace:
   - změna v `kernel/i386/modules/*` neovlivní `x86_64` ani `i386-floppy`.
5. FAT12 negativní test:
   - chybějící `KERNEL32.BIN` => stage2 vypíše chybu a zastaví.

## Assumptions and Defaults
1. Duplicitní moduly mezi větvemi jsou záměr (čistá izolace).
2. `i386-floppy` není Multiboot kompatibilní.
3. `iso-*` zůstávají GRUB-based.
4. Priorita je čitelnost architektur, ne minimalizace duplicit.
