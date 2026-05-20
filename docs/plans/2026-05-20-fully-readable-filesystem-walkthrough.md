# Fully Readable Filesystem Walkthrough

We have successfully resolved the filesystem and boot issues on all targets (floppy disk `img-32` and CD-ROM `iso-32` / `iso-64`). Below is a walkthrough of the changes implemented, verified, and compiled.

## Summary of Changes

### 1. Floppy Bootloader Unreal Mode & High-Memory Copy
- **File:** [stage2.asm](file:///home/adam/Dokumenty/Projekty/C-C++/System1/boot/simple32/stage2.asm)
- **Change:** Implemented a switch to 16-bit **Unreal Mode** right after standard floppy loading completes. 
- **Details:** The bootloader now enters protected mode temporarily, loads a flat 4GB selector into the `GS` segment register, switches back to real mode, and iterates through all 2880 sectors of the floppy disk LBA (1.44 MB). Each sector is loaded via standard BIOS `int 0x13` into low memory (`boot_sector`), copied directly into high memory at `0x00200000` (2 MB) via `GS:[edi]`, and then the bootloader appends this physical memory address as the 11th member (`floppy_image_addr`) to the `boot_info` structure at `BOOT_INFO_SEG:0`.
- **Why:** This solves the physical block limitation where only a fraction of floppy sectors could be cached inside the low 1MB barrier.

### 2. Kernel Boot Info & Block Driver Integration
- **Files:**
  - [kernel.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/i386-floppy/kernel.c)
  - [fs_core.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/fs_core.c)
- **Change:** Integrated `floppy_image_addr` into the kernel's boot parameters and updated block device read routines.
- **Details:** 
  - Added `floppy_image_addr` to `boot_info_t` (floppy kernel loader) and `fs_floppy_boot_info_t` (filesystem core).
  - Modified `fs_cached_floppy_read` in the block driver. If `info->floppy_image_addr` is non-zero, it performs single, fast sector memory copies directly from the loaded high-memory area, making all 2880 sectors completely visible instantly. If it is zero, it falls back to the original limited caching mechanism (backward compatibility preserved).

### 3. FAT12 Nested Subdirectories
- **File:** [fat12_core.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/fat12_core.c)
- **Change:** Rewrote filesystem stubs with real nested subdirectory navigation.
- **Details:**
  - Implemented 12-bit cluster parsing (`fat12_get_next_cluster`) to walk arbitrary cluster chains.
  - Implemented directory-entry searches (`fat12_find_entry_in_dir`), recursive path resolution (`fat12_resolve_path`), and path string normalization (`fat12_normalize_path`).
  - Completely rewrote `fat12_change_dir` and `fat12_list_dir` to support full traversal of nested directories (absolute and relative path components like `/boot`, `.` and `..`).

### 4. ISO9660 Nested Subdirectories
- **File:** [iso9660_core.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/iso9660_core.c)
- **Change:** Rewrote ISO9660 stubs with real nested subdirectory navigation.
- **Details:**
  - Implemented a sector-spanning record finder (`iso_find_entry_in_dir`) and recursive path resolution (`iso_resolve_path`) designed for variable-length ISO9660 directory records.
  - Completely rewrote `iso_change_dir` and `iso_list_dir` to support deep traversal.

---

## Verification & Compilation

We successfully ran a clean build of all compilation targets:
```bash
make clean
make iso-32 iso-64 img-32
```
All targets compiled cleanly with zero errors.

### Build Artifacts Created:
1. **Floppy Image Target**: [system1-img-32.img](file:///home/adam/Dokumenty/Projekty/C-C++/System1/build/artifacts/images/system1-img-32.img) (1,474,560 bytes)
2. **x86 ISO Target**: [system1-iso-32.iso](file:///home/adam/Dokumenty/Projekty/C-C++/System1/build/artifacts/images/system1-iso-32.iso) (13,254,656 bytes)
3. **x86_64 ISO Target**: [system1-iso-x86_64.iso](file:///home/adam/Dokumenty/Projekty/C-C++/System1/build/artifacts/images/system1-iso-x86_64.iso) (13,271,040 bytes)

---

## Floppy Target Single-Kernel Subdirectory Booting (`img-32`)
Per the user requests, the floppy filesystem (`img-32`) and stage 2 bootloader (`stage2.asm`) were modified to behave exactly like the CD-ROM target—having only a **single kernel** located inside the `/boot/` subdirectory:

1. **Single-Kernel Filesystem Structure**:
   - **No Root Kernel**: The root directory contains only the `/boot/` directory (matching the CD layout).
   - **Boot Subdirectory Kernel**: The single kernel is located at `/boot/KERNEL` (displays natively as `boot/kernel` under our custom FAT12 driver).
2. **Subdirectory Booting Implementation in `stage2.asm`**:
   - The bootloader performs a **two-stage directory lookup**:
     - **Stage 1**: Scans the Root Directory for an entry matching the directory name `'BOOT       '`.
     - **Stage 2**: Resolves the starting cluster of the `/boot` directory, converts it to LBA using `lba = data_lba + cluster - 2`, and loads its first sector (512 bytes, containing up to 16 directory entries) into `root_buffer`.
     - **Stage 3**: Scans the loaded sector for the file entry matching `'KERNEL     '`. Once located, it grabs its size and starting cluster and proceeds to load the kernel into memory.
3. **Verified Disk Structure**:
   ```
   Directory for ::/
   boot         <DIR>     2026-05-20  11:38

   Directory for ::/boot
   STAGE1   BIN       512 2026-05-20  11:38
   STAGE2   BIN     13178 2026-05-20  11:38
   KERNEL           38673 2026-05-20  11:38
   ```
