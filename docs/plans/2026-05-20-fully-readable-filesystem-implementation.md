# Fully Readable Filesystem Implementation Plan

**Goal:** Ensure that everything on the disk is visible and accessible on the filesystem (`fs` module), and that both the multiboot/GRUB and custom floppy loaders boot correctly.

**Architecture:**
1. **Floppy Image Loading**: Modify `boot/simple32/stage2.asm` to enter Unreal Mode and load the entire 1.44MB floppy disk image (2880 sectors) into high memory at `0x00200000` (2 MB). Pass this address (`floppy_image_addr`) to the kernel in the `boot_info` structure.
2. **Global Floppy Sector Read**: Update `fs_cached_floppy_read` in `fs_core.c` to read directly from the RAM-disk copy at `floppy_image_addr` when present.
3. **FAT12 Nested Subdirectories**: Complete path resolution and traversal in `fat12_core.c` using cluster chains and parsing standard subdirectory tables.
4. **ISO9660 Nested Subdirectories**: Complete path resolution and traversal in `iso9660_core.c` by reading directory sectors and variable-length ISO9660 directory records.

---

### Task 1: Pass Floppy Image Address to Kernel

**Files:**
- Modify: [stage2.asm](file:///home/adam/Dokumenty/Projekty/C-C++/System1/boot/simple32/stage2.asm)
- Modify: [kernel.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/i386-floppy/kernel.c)

**Steps:**
1. Add `enter_unreal` helper in `stage2.asm` to enter Unreal Mode and load the flat 4GB selector `0x10` into `GS`.
2. After standard kernel loading, enter Unreal Mode.
3. Initialize `floppy_target` at `0x00200000` and loop `current_lba` from `0` to `2879`:
   - Call `read_lba_sector` to read the sector into `boot_sector` in low memory.
   - Copy 512 bytes from `boot_sector` to `GS:[floppy_target]`.
   - Increment `floppy_target` by 512.
4. In `done_loading`, append `0x00200000` to the boot info structure at `BOOT_INFO_SEG:0`.
5. Update `boot_info_t` in `kernel/i386-floppy/kernel.c` to include `uint32_t floppy_image_addr`.

---

### Task 2: Expose All Floppy Sectors in Block Driver

**Files:**
- Modify: [fs_core.h](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/fs_core.h)
- Modify: [fs_core.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/fs_core.c)

**Steps:**
1. Add `uint32_t floppy_image_addr` to `fs_floppy_boot_info_t` in `fs_core.h`.
2. Update `fs_cached_floppy_read` in `fs_core.c` to check if `info->floppy_image_addr` is not 0.
3. If not 0, read sectors directly from `info->floppy_image_addr + lba * 512`.
4. Fall back to original limited cache if `floppy_image_addr` is 0 (backward compatibility).

---

### Task 3: Implement FAT12 Nested Subdirectory Navigation

**Files:**
- Modify: [fat12_core.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/fat12_core.c)

**Steps:**
1. Keep track of current CWD cluster `g_fat12_cwd_cluster` (initially 0 for root).
2. Implement `fat12_get_next_cluster(uint32_t cluster)` to read and parse 12-bit values from the FAT table.
3. Implement `fat12_resolve_path(const char* path, uint32_t* out_cluster, uint8_t* out_is_dir)` to parse nested relative/absolute components (natively handling `.` and `..` via directory records).
4. Implement `fat12_normalize_path(char* dst, const char* src)` to build normalized absolute paths for the CWD string.
5. Update `fat12_change_dir` to use `fat12_resolve_path` and `fat12_normalize_path`.
6. Update `fat12_list_dir` to read and output the directory records of subdirectories based on their cluster chain.

---

### Task 4: Implement ISO9660 Nested Subdirectory Navigation

**Files:**
- Modify: [iso9660_core.c](file:///home/adam/Dokumenty/Projekty/C-C++/System1/kernel/common/modules/fs/iso9660_core.c)

**Steps:**
1. Keep track of CWD LBA `g_iso_cwd_lba` and CWD size `g_iso_cwd_size` (initially root LBA and size).
2. Implement `iso_resolve_path(const char* path, uint32_t* out_lba, uint32_t* out_size, uint8_t* out_is_dir)` to parse path components and traverse variable-length ISO9660 subdirectory records.
3. Implement `iso_normalize_path(char* dst, const char* src)` for path normalization.
4. Update `iso_change_dir` to resolve directories and update LBA, size, and CWD.
5. Update `iso_list_dir` to list variable-length records of subdirectories.

---

### Task 5: Compilation and Interactive Testing

**Steps:**
1. Compile the targets to ensure zero errors/warnings:
   ```bash
   make clean
   make iso-32 iso-64 img-32
   ```
2. Run manual QEMU regression check on floppy boot `img-32`:
   - Boot `build/artifacts/images/system1-img-32.img`.
   - Verify `pwd` returns `/`.
   - Verify `ls` shows `boot` directory and `kernel32.bin`.
   - Run `cd boot` followed by `pwd` (should show `/boot`).
   - Run `ls` (should show `kernel.bin`, `stage1.bin`, `stage2.bin`!).
   - Verify `cd ..` goes back to root.
3. Run manual QEMU regression check on ISO `iso-32` and `iso-64` targets.
