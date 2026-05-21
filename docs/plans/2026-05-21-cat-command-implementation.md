# Cat Command Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `cat <file>` to the System/1 shell and preserve readable file contents through the boot-media-to-RAMFS import path.

**Architecture:** Extend the existing VFS driver contract with `read_file(path, buffer, cap, out_size)`. Implement the operation in ISO9660 and FAT12, keep RAMFS as metadata for imported files, and route shell reads to the original boot media on demand. Keep the command option-free and use existing FS error reporting.

**Tech Stack:** Freestanding C, System/1 shell module, VFS/RAMFS/FAT12/ISO9660 modules, Makefile module build targets.

---

### Task 1: Add VFS Read Contract

**Files:**
- Modify: `kernel/common/modules/fs/vfs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs.c`
- Modify: `kernel/i386/modules/fs/fs.h`
- Modify: `kernel/x86_64/modules/fs/fs.h`
- Modify: `kernel/i386-floppy/modules/fs/fs.h`

**Step 1: Write the failing build expectation**

Run: `make modules-32`

Expected before implementation: no `fs_read_file` symbol exists for shell code once Task 4 starts using it.

**Step 2: Add declarations**

Add:

```c
int fs_core_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);
int fs_read_file(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);
```

Add this callback to `vfs_driver_t`:

```c
int (*read_file)(const char* path, char* buffer, uint32_t cap, uint32_t* out_size);
```

**Step 3: Add wrapper implementation**

In `fs.c`, forward `fs_read_file()` to `fs_core_read_file()`.

### Task 2: Keep RAMFS File Metadata

**Files:**
- Modify: `kernel/common/modules/fs/ramfs_core.h`
- Modify: `kernel/common/modules/fs/ramfs_core.c`

**Step 1: Keep file nodes as metadata**

Use existing imported RAMFS file nodes for directory listings, but do not copy all boot-media file bytes into RAMFS during mount.

**Step 2: Add read callback placeholder**

Add `ramfs_core_read_file()` for VFS shape compatibility. It resolves the path for correct missing-path and directory errors, but returns `FS_ERR_INVALID` for existing RAMFS-only files because file content is supplied by the original media driver.

### Task 3: Implement Media Reads And Import Contents

**Files:**
- Modify: `kernel/common/modules/fs/iso9660_core.c`
- Modify: `kernel/common/modules/fs/fat12_core.c`
- Modify: `kernel/common/modules/fs/fs_core.c`

**Step 1: ISO9660 read**

Use `iso_resolve_path()` to find file LBA and size. Reject directories. Read 2048-byte sectors and copy only the requested file size into the caller buffer.

**Step 2: FAT12 read**

Extend FAT12 path resolution or add an entry resolver that also returns file size. Walk the cluster chain and copy up to the file size from 512-byte sectors.

**Step 3: Preserve metadata import**

In `fs_import_media_dir()`, keep importing file nodes with `ramfs_core_import_file()`. Do not read each file during mount.

### Task 4: Add Shell Command

**Files:**
- Modify: `kernel/common/modules/shell/shell_core.c`

**Step 1: Register command**

Add `cat` to `g_shell_commands` and `help` output.

**Step 2: Implement handler**

Add `shell_cmd_cat(argv, argc)` with one required path. Read into a fixed buffer, print bytes with `vga_putc`, and add a trailing newline only if the file does not already end in one.

**Step 3: Dispatch command**

Add a `cat` branch in `shell_core_run()`.

### Task 5: Verify

**Files:**
- No source edits.

**Step 1: Build all module targets**

Run: `make modules-32 modules-64 modules-img-32`

Expected: all module objects compile without warnings or errors.

**Step 2: Optional image build**

Run: `make iso-32 iso-64 img-32`

Expected: all boot images are produced.
