# Simple Paging-Based Memory Manager Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a simple paging-backed memory manager for System/1 that provides page allocation, `kmalloc`/`kfree`, boot memory stats, and a shell `mmstat` command.

**Architecture:** Add a shared `mm` module that works inside the existing 64 MiB identity-mapped paging window. The module owns a physical page bitmap and a first-fit kernel heap that grows by taking 4 KiB pages from the page allocator.

**Tech Stack:** Freestanding C, existing shared module pattern (`kernel/common/modules/*` plus arch headers), current paging API, VGA/klog shell diagnostics.

---

### Task 1: MM Module API

**Files:**
- Create: `kernel/common/modules/mm/mm.c`
- Create: `kernel/common/modules/mm/mm_core.c`
- Create: `kernel/common/modules/mm/mm_core.h`
- Create: `kernel/i386/modules/mm/mm.h`
- Create: `kernel/x86_64/modules/mm/mm.h`
- Create: `kernel/i386-floppy/modules/mm/mm.h`

**Steps:**
1. Define `mm_stats_t` with page totals and heap totals.
2. Expose:
   - `int mm_init(uint32_t boot_magic, uint32_t boot_info_ptr);`
   - `void* kmalloc(uint32_t size);`
   - `void kfree(void* ptr);`
   - `void* mm_alloc_page(void);`
   - `void mm_free_page(void* page);`
   - `void mm_get_stats(mm_stats_t* out);`
   - `void mm_print_stats(void);`
3. Keep `mm.c` as a thin wrapper around `mm_core`.
4. Use the same header guard style as the existing modules.

### Task 2: Kernel Bounds

**Files:**
- Modify: `linker/linker.i386.ld`
- Modify: `linker/linker.x86_64.ld`
- Modify: `linker/linker.floppy.i386.ld`

**Steps:**
1. Add `__kernel_start = .;` at the beginning of the load region.
2. Align the final location to 4 KiB and add `__kernel_end = .;`.
3. Use these symbols in MM to reserve the loaded kernel image.

### Task 3: Physical Page Allocator

**Files:**
- Modify: `kernel/common/modules/mm/mm_core.c`

**Steps:**
1. Manage only pages below `paging_identity_limit()`.
2. For Multiboot2, parse memory map tag type `6` and mark usable frames.
3. For floppy/generic fallback, mark `[1 MiB, 64 MiB)` usable.
4. Reserve:
   - page 0 and all memory below 1 MiB
   - kernel range `__kernel_start..__kernel_end`
   - Multiboot2 info structure
   - Multiboot2 modules
   - MM metadata/static tables
5. Implement first-free-page allocation and free with page alignment checks.

### Task 4: PMM-Backed Heap

**Files:**
- Modify: `kernel/common/modules/mm/mm_core.c`

**Steps:**
1. Implement first-fit heap blocks with header `{ size, free, next, prev }`.
2. Align `kmalloc` sizes to `sizeof(uintptr_t)`.
3. Split large free blocks and coalesce adjacent free blocks in `kfree`.
4. Grow heap by allocating enough full pages from PMM for the requested block.
5. Return `0` on allocation failure.
6. Treat `kfree(0)` as a no-op.

### Task 5: Boot Integration and Stats

**Files:**
- Modify: `kernel/i386/kernel.c`
- Modify: `kernel/x86_64/kernel.c`
- Modify: `kernel/i386-floppy/kernel.c`

**Steps:**
1. Include `mm.h`.
2. Call `mm_init(magic, info_ptr)` immediately after `paging_init(...)`.
3. Panic on MM init failure.
4. Print MM stats during boot using hex page/byte values.

### Task 6: Shell `mmstat`

**Files:**
- Modify: `kernel/common/modules/shell/shell_core.c`

**Steps:**
1. Include `mm.h`.
2. Add `mmstat` to command list and `help` output.
3. Add `shell_cmd_mmstat()` that calls `mm_print_stats()`.
4. Preserve existing command behavior.

### Task 7: Documentation

**Files:**
- Modify: `MODULE_CONTRACTS.md`
- Modify: `CONTEXT.TXT`

**Steps:**
1. Document the `mm` module API.
2. Document V1 limits: identity mapping only, 64 MiB max, no virtual map/unmap, no userspace.
3. Note that RAMFS static pools are not migrated in this task.

### Task 8: Verification

**Files:**
- None

**Steps:**
1. Red check before implementation: `make modules-32 MODULES=mm` should fail because the module does not exist.
2. Module builds:
   - `make modules-32`
   - `make modules-64`
   - `make modules-img-32`
3. Full builds:
   - `make iso-32 iso-64 img-32`
4. QEMU smoke:
   - boot each target to shell
   - confirm MM boot stats are printed
   - run `mmstat`
   - run `ls`, `cd Documents`, `cat file.txt`, and `ticks`

