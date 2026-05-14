# Read-Only Disk RootFS Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Mount a read-only filesystem from boot media as `/` on every target and panic with `panic("Unable to mount FS");` if the mount fails.

**Architecture:** Introduce a small read-only VFS boundary under the existing public `fs_*` API. Add block/media discovery and read-only FAT12 and ISO9660 filesystem drivers, with the existing RAM rootfs removed from normal boot fallback behavior.

**Tech Stack:** freestanding C, static allocation, existing common-module pattern, FAT12, ISO9660, Makefile/QEMU verification.

---

### Task 1: Add Read-Only FS Semantics

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/i386/modules/fs/fs.h`
- Modify: `kernel/x86_64/modules/fs/fs.h`
- Modify: `kernel/i386-floppy/modules/fs/fs.h`
- Modify: `kernel/common/modules/shell/shell_core.c`

**Steps:**
1. Add `FS_ERR_READ_ONLY`.
2. Update shell FS error printing to show `read-only filesystem`.
3. Change `fs_core_make_dir()` to return `FS_ERR_READ_ONLY` after disk root mounting is introduced.
4. Verify module compile catches every header copy.
5. Commit: `fs: add read-only filesystem error`.

### Task 2: Split Current RAM RootFS From Public FS API

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.c`
- Create: `kernel/common/modules/fs/ramfs_core.c`
- Create: `kernel/common/modules/fs/ramfs_core.h`

**Steps:**
1. Move current node-tree implementation into `ramfs_core.*`.
2. Keep `fs_core.c` as the public dispatcher layer.
3. Preserve behavior temporarily so existing shell commands still work.
4. Build all module targets.
5. Commit: `fs: split ramfs from fs dispatcher`.

### Task 3: Add VFS Driver Interface

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Create: `kernel/common/modules/fs/vfs_core.h`

**Steps:**
1. Define driver callbacks for `list_dir`, `change_dir`, `get_cwd_path`, and `make_dir`.
2. Add a single mounted root driver pointer in `fs_core.c`.
3. Route public `fs_core_*` calls through the mounted driver.
4. Keep RAMFS as a temporary driver for red/green verification only.
5. Commit: `fs: add readonly vfs dispatch`.

### Task 4: Add Block Device Boundary

**Files:**
- Create: `kernel/common/modules/fs/block_core.h`
- Create: `kernel/common/modules/fs/block_core.c`

**Steps:**
1. Define `block_device_t` with `sector_size`, `sector_count`, `read`.
2. Implement probe registration for one boot/root device.
3. Add target stubs that return no device until real media readers are wired.
4. Verify `fs_init()` fails cleanly when no root device exists.
5. Commit: `fs: add block device boundary`.

### Task 5: Add FAT12 Read-Only Driver

**Files:**
- Create: `kernel/common/modules/fs/fat12_core.h`
- Create: `kernel/common/modules/fs/fat12_core.c`

**Steps:**
1. Write tests or host-checkable helpers for BPB parsing.
2. Parse FAT12 BPB fields needed for root directory.
3. Implement root directory listing from 8.3 entries.
4. Implement directory cluster traversal for subdirectories.
5. Return `FS_ERR_READ_ONLY` for mutation.
6. Commit: `fs: add readonly fat12 driver`.

### Task 6: Add ISO9660 Read-Only Driver

**Files:**
- Create: `kernel/common/modules/fs/iso9660_core.h`
- Create: `kernel/common/modules/fs/iso9660_core.c`

**Steps:**
1. Write tests or host-checkable helpers for primary volume descriptor parsing.
2. Parse sector 16 primary volume descriptor.
3. Extract root directory record.
4. Implement directory listing with normalized names.
5. Return `FS_ERR_READ_ONLY` for mutation.
6. Commit: `fs: add readonly iso9660 driver`.

### Task 7: Wire Floppy Boot Media

**Files:**
- Create: `kernel/i386-floppy/modules/fs/bootmedia.c`
- Modify: `kernel/i386-floppy/modules/fs/fs.h`
- Modify: `kernel/common/modules/fs/block_core.c`

**Steps:**
1. Add i386-floppy sector read backend for boot floppy media.
2. Expose it as the root block device.
3. Mount FAT12 as `/` on `i386-floppy`.
4. Verify `make img-32`.
5. Runtime smoke: `ls /` shows FAT12 image contents.
6. Commit: `fs: mount floppy fat12 root`.

### Task 8: Wire ISO Boot Media

**Files:**
- Create: `kernel/i386/modules/fs/bootmedia.c`
- Create: `kernel/x86_64/modules/fs/bootmedia.c`
- Modify: `boot/grub/i386/grub.cfg`
- Modify: `boot/grub/x86_64/grub.cfg`
- Modify: `kernel/i386/kernel.c`
- Modify: `kernel/x86_64/kernel.c`

**Steps:**
1. Prefer the simplest reliable boot-media path for GRUB targets.
2. If direct CD/DVD sector access is not available yet, pass an ISO root image/module pointer from Multiboot2 metadata as the first implementation checkpoint.
3. Mount ISO9660 as `/` for both GRUB targets.
4. Verify `make iso-32 iso-64`.
5. Runtime smoke: `ls /` shows ISO contents.
6. Commit: `fs: mount iso9660 root for grub targets`.

### Task 9: Make Mount Failure Fatal

**Files:**
- Modify: `kernel/i386/kernel.c`
- Modify: `kernel/x86_64/kernel.c`
- Modify: `kernel/i386-floppy/kernel.c`
- Modify: `kernel/common/modules/fs/fs_core.c`

**Steps:**
1. Ensure `fs_init()` returns failure if no disk root was mounted.
2. Replace existing panic strings with exactly `panic("Unable to mount FS");`.
3. Remove normal boot RAMFS fallback.
4. Verify all targets still build.
5. Commit: `kernel: panic when root fs cannot mount`.

### Task 10: Documentation And Regression

**Files:**
- Modify: `PLAN.MD`
- Modify: `CONTEXT.TXT`

**Steps:**
1. Document read-only disk root behavior.
2. Document supported target/media mapping.
3. Run:

```bash
make modules-32 modules-64 modules-img-32
make iso-32 iso-64 img-32
```

4. Run QEMU smoke tests where available.
5. Commit: `docs: document readonly disk rootfs`.
