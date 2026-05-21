# Write-Back RAMFS Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Boot CD and floppy roots into a writable RAMFS working tree, then handle persistence at shutdown with CD read-only errors and floppy write-back.

**Architecture:** Keep the current shell-facing `fs_*` API and existing media discovery. Mount the boot media read-only, import its visible directory tree into `ramfs_core`, switch the root VFS driver to RAMFS, and add a shutdown persistence hook. Floppy persistence is isolated behind an FS write-back backend so RAMFS editing and physical disk writes do not leak into shell code.

**Tech Stack:** Freestanding C, existing System/1 VFS/block/FAT12/ISO9660 modules, custom i386 floppy loader, QEMU manual checks.

---

### Task 1: RAMFS Import And Dirty Tracking

**Files:**
- Modify: `kernel/common/modules/fs/ramfs_core.h`
- Modify: `kernel/common/modules/fs/ramfs_core.c`

**Step 1: Add API declarations**

Add:

```c
int ramfs_core_reset_empty(void);
int ramfs_core_import_dir(const char* path);
int ramfs_core_import_file(const char* path);
uint8_t ramfs_core_is_dirty(void);
void ramfs_core_clear_dirty(void);
const vfs_driver_t* ramfs_core_driver(void);
```

**Step 2: Verify compile fails before implementation**

Run: `make modules-img-32`

Expected: fails once callers are added in Task 2 because the new symbols are missing.

**Step 3: Implement minimal RAMFS helpers**

- Split bootstrap init from empty init.
- Add `ramfs_core_reset_empty()` that creates only `/`.
- Add `ramfs_core_import_dir()` and `ramfs_core_import_file()` that create nodes by absolute path without marking dirty.
- Set dirty only inside user mutation paths such as `ramfs_core_make_dir()`.
- Export a `vfs_driver_t` for RAMFS.

**Step 4: Verify module build**

Run: `make modules-img-32`

Expected: all floppy module objects compile.

---

### Task 2: Import Boot Media Into RAMFS

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs.c`
- Modify arch wrapper headers as needed:
  - `kernel/i386/modules/fs/fs.h`
  - `kernel/x86_64/modules/fs/fs.h`
  - `kernel/i386-floppy/modules/fs/fs.h`

**Step 1: Add a recursive importer**

After `fs_probe_block_root()` mounts FAT12 or ISO9660, traverse the mounted read-only driver with `list_dir`, importing each visible directory and file name into RAMFS.

**Step 2: Preserve root listing shape**

Root `ls` after RAMFS switch must match the mounted media listing. Do not add the old RAMFS bootstrap directories unless they exist on media.

**Step 3: Switch root driver**

When import succeeds:

```c
g_media_driver = g_root_driver;
ramfs_core_reset_empty();
fs_import_tree("/");
g_root_driver = ramfs_core_driver();
```

**Step 4: Verify build**

Run: `make modules-32 modules-64 modules-img-32`

Expected: all three module target groups compile.

---

### Task 3: Shutdown Persistence Hook

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/fs/fs.c`
- Modify wrapper headers listed in Task 2.
- Modify: `kernel/common/modules/shell/shell_core.c`

**Step 1: Add public shutdown API**

Add:

```c
int fs_shutdown(void);
```

**Step 2: CD behavior**

If RAMFS is dirty and the original media is ISO9660, return `FS_ERR_READ_ONLY`.

**Step 3: Floppy prompt**

For dirty floppy RAMFS, `shell_cmd_shutdown()` asks:

```text
Write changes to floppy? [y/N]
```

Only `y` or `Y` triggers write-back. Other answers continue shutdown without saving.

**Step 4: Initial floppy backend**

Return a clear placeholder error until physical FAT12 serialization and FDC write support are implemented. Keep this isolated behind `fs_core_flush_to_boot_media()`.

**Step 5: Verify build**

Run: `make modules-32 modules-64 modules-img-32`

Expected: all module target groups compile.

---

### Task 4: FAT12 Metadata Serialization

**Files:**
- Modify: `kernel/common/modules/fs/fat12_core.h`
- Modify: `kernel/common/modules/fs/fat12_core.c`
- Modify: `kernel/common/modules/fs/fs_core.c`

**Step 1: Add a constrained FAT12 writer**

Support creating directories that fit in the existing FAT12 image. Start with root-directory and one-level directory creation; return `FS_ERR_NO_SPACE` when FAT/root capacity is insufficient.

**Step 2: Serialize RAMFS directory additions**

Compare imported tree versus current RAMFS tree, allocate FAT12 entries for new directories, and update the RAM disk image.

**Step 3: Verify image-level behavior**

Run: `make img-32`

Manual expected: boot floppy, `mkdir test`, shutdown yes, reboot, `test/` appears.

---

### Task 5: Physical Floppy Write And Locked-Disk Error

**Files:**
- Create or modify within existing module layout only if needed:
  - `kernel/common/modules/fs/block_core.h`
  - `kernel/common/modules/fs/block_core.c`
  - `kernel/common/modules/fs/fs_core.c`

**Step 1: Add optional block write callback**

Extend `block_device_t` with:

```c
int (*write)(block_device_t* dev, uint32_t lba, uint32_t count, const void* buffer);
```

Existing read-only devices leave it null.

**Step 2: Add floppy sector write backend**

Use the i386 floppy controller path to write dirty sectors from the RAM image back to drive A. Map write-protect failures to `FS_ERR_READ_ONLY`.

**Step 3: Verify locked floppy**

Run QEMU with the floppy image read-only and repeat `mkdir test`, `shutdown`, answer yes.

Expected: save reports read-only failure and shutdown continues only after reporting it.

---

### Task 6: Final Verification

**Files:**
- No source edits.

**Step 1: Build all targets**

Run: `make all`

Expected: `system1-iso-32.iso`, `system1-iso-x86_64.iso`, and `system1-img-32.img` are produced.

**Step 2: Manual QEMU checks**

- CD 32-bit: `mkdir test`, `ls`, `shutdown`.
- CD 64-bit: same.
- FD: `mkdir test`, `shutdown`, answer yes, reboot.
- FD locked: same save flow, expect read-only error.

**Step 3: Document gaps**

If physical floppy write-back is not fully supported by the emulator or controller path, document the exact failing command/output and leave RAMFS shutdown behavior intact.
