# Paging V1 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Zavést Paging V1 (identity map 64 MiB, 4 KiB stránky) pro `iso-32`, `iso-64` a `img-32` bez regresí v shell/IRQ flow.

**Architecture:** Přidá se nový shared modul `paging` s arch wrapper hlavičkami. `paging_init(magic, info)` provede boot-source detekci (Multiboot2 vs floppy fallback), připraví page tables a zapne/přepne paging. Page fault vector `14` bude mít dedikovanou diagnostiku přes `CR2` + panic.

**Tech Stack:** freestanding C, x86 asm přes inline asm, existující module pattern (`common/modules/*` + `kernel/*/modules/*`), Makefile auto-modules.

---

### Task 1: Modul skeleton + API kontrakt

**Files:**
- Create: `kernel/common/modules/paging/paging.c`
- Create: `kernel/common/modules/paging/paging_core.c`
- Create: `kernel/common/modules/paging/paging_core.h`
- Create: `kernel/i386/modules/paging/paging.h`
- Create: `kernel/x86_64/modules/paging/paging.h`
- Create: `kernel/i386-floppy/modules/paging/paging.h`

**Steps:**
1. Přidat veřejné API:
   - `int paging_init(uint32_t boot_magic, uint32_t boot_info_ptr);`
   - `uint8_t paging_is_enabled(void);`
   - `uintptr_t paging_identity_limit(void);`
   - `void paging_handle_page_fault(void);`
2. V `paging.c` držet jen wrapper volání do `paging_core`.
3. Header guardy držet ve stylu repozitáře.

### Task 2: Paging core (i386 + floppy)

**Files:**
- Modify: `kernel/common/modules/paging/paging_core.c`
- Modify: `kernel/common/modules/paging/paging_core.h`

**Steps:**
1. Přidat konstanty V1:
   - page size `4096`
   - identity limit `0x04000000` (64 MiB)
2. Vytvořit statické, 4KiB zarovnané struktury:
   - `pd[1024]`
   - `pt[16][1024]`
3. Naplnit PDE/PTE pro identity map 0..64 MiB (present+rw).
4. Nahrát `CR3`, nastavit `CR0.PG`.
5. Stavová proměnná `g_paging_enabled` + idempotentní `paging_init`.

### Task 3: Paging core (x86_64 rebase na 4 KiB tables)

**Files:**
- Modify: `kernel/common/modules/paging/paging_core.c`

**Steps:**
1. Přidat 4KiB zarovnané tabulky:
   - `pml4[512]`, `pdpt[512]`, `pd[512]`, `pt[32][512]`
2. Naplnit identity map 0..64 MiB (present+rw) přes 4-level walk.
3. Přepnout `CR3` na nový PML4 (long mode zůstává aktivní).
4. Zachovat společné API a stav jako v i386.

### Task 4: Boot-source a fallback logika

**Files:**
- Modify: `kernel/common/modules/paging/paging_core.c`
- Modify: `kernel/i386-floppy/kernel.c`

**Steps:**
1. Přidat MB2 magic kontrolu (`0x36d76289`) a minimální parser memory map tagu.
2. Pro MB2 cestu mapovat max usable rámce do 64 MiB okna (identity V1).
3. Pro floppy cestu použít konzervativní fallback usable `[1 MiB, 64 MiB)`.
4. Rezervovat low-memory `<1 MiB` a region kernelu podle boot vstupu.
5. Logovat zvolený init path (`mb2` vs `fallback`).

### Task 5: Integrace do kernel boot flow

**Files:**
- Modify: `kernel/i386/kernel.c`
- Modify: `kernel/x86_64/kernel.c`
- Modify: `kernel/i386-floppy/kernel.c`

**Steps:**
1. Přidat include `paging.h`.
2. Po `vga_init()` volat `paging_init(magic, info_ptr)`.
3. Při failu zalogovat fatální chybu a zastavit kernel.
4. Ponechat pořadí init tak, aby IRQ/shell běžely po pagingu.

### Task 6: Page fault handler napojení

**Files:**
- Modify: `kernel/i386/modules/interrupts/interrupts.c`
- Modify: `kernel/x86_64/modules/interrupts/interrupts.c`
- Modify: `kernel/i386-floppy/modules/interrupts/interrupts.c`

**Steps:**
1. V `interrupts_dispatch` přidat větev pro vector `14`.
2. Volat `paging_handle_page_fault()` a následně ukončit přes panic path.
3. IRQ dispatch (`32..47`) nechat beze změny.

### Task 7: Dokumentace a kontrakty

**Files:**
- Modify: `MODULE_CONTRACTS.md`
- Modify: `CONTEXT.TXT`
- Modify: `PLAN.MD`

**Steps:**
1. Dopsat `paging` modul kontrakt a API.
2. Zapsat omezení V1 (identity only, 64 MiB, bez userspace/higher-half).
3. Zapsat open items pro V2 (`pmm`, higher-half, user split).

### Task 8: Verifikace

**Files:**
- None

**Steps:**
1. Build: `make clean && make iso-32 iso-64 img-32`.
2. Smoke run každého targetu (boot + prompt `kernel> `).
3. Ověřit, že `ticks` roste a keyboard IRQ funguje.
4. Zkontrolovat paging log (`enabled`, `limit=64 MiB`).
5. Reportnout přesné výstupy a případné blokery.
