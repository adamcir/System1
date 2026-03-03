# System/1: C-first build plán s 3 výstupy (`iso-32`, `iso-x86_64`, `img-32`)

## Shrnutí
Projekt bude mít dvě boot cesty:
1. `GRUB2 + Multiboot2` pro ISO výstupy:
   - `iso-32` (i386 kernel)
   - `iso-x86_64` (x86_64 kernel)
2. `Jednoduchý vlastní bootloader` pro disketový obraz:
   - `img-32` (i386, floppy image)

Jádro zůstane hlavně v C; assembler jen pro nevyhnutelné low-level přechody.

## Cíl a kritéria úspěchu
1. `make iso-32` vytvoří bootovatelný i386 ISO přes GRUB.
2. `make iso-x86_64` vytvoří bootovatelný x86_64 ISO přes GRUB.
3. `make img-32` vytvoří bootovatelný disketový obraz s jednoduchým bootloaderem.
4. `make run-iso-32`, `make run-iso-x86_64`, `make run-img-32` fungují v QEMU.
5. Ve všech režimech se zobrazí boot banner a základní init stav.

## Architektura a výstupy

## 1) Výstupní artefakty
1. `build/system1-iso-32.iso`
2. `build/system1-iso-x86_64.iso`
3. `build/system1-img-32.img` (1.44MB floppy)

## 2) Boot cesty
1. `GRUB ISO 32-bit`:
   - GRUB načte `kernel32.elf` (Multiboot2), vstup do `entry_i386`.
2. `GRUB ISO 64-bit`:
   - GRUB načte `kernel64.elf` (Multiboot2), vstup v 32-bit, pak přepnutí do long mode, pak C entry.
3. `Simple bootloader img-32`:
   - `stage1` (boot sektor) načte `stage2`.
   - `stage2` načte 32-bit kernel do paměti a skočí na C entry.
   - Bez filesystemu: fixní sektory (MVP).

## 3) Sdílení kódu
1. `kernel/common/*` sdílené C moduly (print/panic/init pomocné části).
2. `arch/i386/*` pro i386 specifika.
3. `arch/x86_64/*` pro x86_64 specifika.
4. `boot/simple32/*` pro vlastní floppy bootloader.

## Veřejná rozhraní / API / typy

## 1) Entrypointy
1. `void kmain_i386(uint32_t boot_magic, uint32_t boot_info);`
2. `void kmain_x86_64(uint32_t boot_magic, uint32_t boot_info);`
3. `void panic(const char* msg) __attribute__((noreturn));`

## 2) Arch init API
1. `void gdt_init_i386(void);`
2. `void idt_init_i386(void);`
3. `void gdt_init_x86_64(void);`
4. `void idt_init_x86_64(void);`

## 3) Output API
1. `void vga_putc(char c);`
2. `void vga_puts(const char* s);`
3. `void vga_hex_u32(uint32_t v);`
4. `void vga_hex_u64(uint64_t v);`

## 4) Důležité typy
1. `struct gdt_entry32`, `struct idt_entry32`
2. `struct gdt_entry64`, `struct idt_entry64`
3. `struct regs32`, `struct regs64`

## Build systém
1. `Makefile` cíle:
   - `iso-32`, `iso-x86_64`, `img-32`
   - `run-iso-32`, `run-iso-x86_64`, `run-img-32`
   - `clean`, `all`
2. Toolchain:
   - i386: `i686-elf-gcc`, `i686-elf-ld`
   - x86_64: `x86_64-elf-gcc`, `x86_64-elf-ld`
   - bootloader: `nasm`
   - ISO: `grub-mkrescue`
3. `all` defaultně postaví všechny tři artefakty.

## Testy a scénáře

## 1) Artefaktové testy
1. Po buildu existují přesné 3 soubory: `iso-32`, `iso-x86_64`, `img-32`.
2. Každý artefakt je spustitelný v odpovídajícím QEMU targetu.

## 2) Boot testy
1. ISO-32: boot banner + `GDT ok`, `IDT ok`.
2. ISO-x86_64: boot banner + `Long mode ok`, `IDT ok`.
3. IMG-32: boot banner + `Simple loader ok`, kernel start ok.

## 3) Stabilita
1. Žádný triple fault při normálním bootu.
2. Ruční `int3` test (aspoň pro i386 pipeline) projde přes C dispatcher.

## Implementační pořadí
1. Založit adresářovou strukturu + Makefile s 3 pipeline.
2. Rozchodit `iso-32`.
3. Rozchodit `iso-x86_64` (včetně long mode switch).
4. Rozchodit `img-32` (stage1/stage2 + kernel load).
5. Sjednotit sdílené C moduly a test run cíle.
6. Dopsat minimální diagnostické výpisy a validaci.

## Explicitní předpoklady a defaulty
1. Jazykově “hlavně C”: ASM pouze entry/ISR/CPU mode transitions.
2. `img-32` je pouze i386 (disketa), bez x86_64 varianty.
3. Jednoduchý bootloader je MVP bez filesystemu, s fixními sektory.
4. Scheduler, user-space, FS a ovladače nejsou v tomto milníku.
