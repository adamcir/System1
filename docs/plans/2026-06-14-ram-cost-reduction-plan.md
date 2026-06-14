# RAM Cost Reduction Plan

## Goal

Reduce System/1 RAM usage without breaking the current POSIX-like shell and write-back behavior.

The current filesystem model is intentionally simple: boot media is mounted read-only, a RAMFS tree becomes the writable root, unchanged file content can be read from the original boot media, and dirty files are stored in RAMFS until shutdown. That is practical, but it still pays avoidable RAM costs for duplicated metadata, fixed-size file buffers, path strings, dirty path lists, and static global scratch buffers.

The target is not to copy Linux immediately. The target is a staged path from the current RAMFS/fallback model toward a small VFS cache that loads only what is needed and evicts what can be re-read from disk.

## Hard Low-Memory Target

System/1 must keep a viable profile for machines with:

- 1 MB total RAM
- 1.44 MB floppy boot media
- no assumption that the whole disk image can live in memory
- no assumption that the heap can absorb large temporary buffers

This matters because a 1.44 MB floppy image is already larger than the entire 1 MB RAM target. Any design that stores a full floppy image in RAM is invalid for the low-memory profile, even if it works in QEMU with more memory.

Low-memory rules:

- Never require a full 1.44 MB floppy image in RAM.
- Keep filesystem scratch buffers sector-sized or cluster-sized.
- Prefer streaming reads/writes over whole-file or whole-image copies.
- Keep caches optional and bounded.
- Keep the bootable shell usable even if write-back cache capacity is exhausted.
- Return explicit errors such as `ENOSPC`/`ENOMEM` instead of corrupting data.
- Make every static buffer visible in `fsstat` or `mmstat`.

Initial RAM budget targets for the FS layer:

- block/cache buffers: <= 4 KiB in low-memory profile
- dirty metadata: <= 2 KiB before node-flag migration
- RAMFS node metadata: <= 16 KiB before lazy import
- writable file data resident at once: <= 16 KiB before heap storage
- single write-back scratch buffer: 512 bytes, or one FAT cluster if cluster size is larger

These are targets, not permanent architectural limits. The important part is that the low-memory build has a bounded profile and refuses operations that exceed it.

## Non-Goals

- Do not add a full Linux-style page cache in the first pass.
- Do not require dynamic allocation everywhere before the MM layer is ready for it.
- Do not break floppy and ISO boot targets.
- Do not remove RAMFS write-back until an equivalent persistent mutation path exists.
- Do not implement permissions, journaling, mmap, or full block cache policy in this plan.

## Current RAM Costs

- RAMFS allocates fixed-size node structures for every imported directory and file.
- Each RAMFS file node contains a fixed `RAMFS_FILE_CAP` data buffer, even for empty imported placeholder files.
- Boot media metadata remains available through FAT12/ISO drivers while imported RAMFS metadata duplicates part of it.
- Dirty tracking stores full normalized paths in fixed arrays.
- Write-back uses a fixed global file scratch buffer.
- Open file tables and process tables use fixed arrays.
- Imported unchanged files can be represented twice: as a RAMFS node and as a readable boot-media entry.
- The current floppy path may depend on `floppy_image_addr`, which is not acceptable for a 1 MB RAM target if it means the whole 1.44 MB image is resident.

## Success Metrics

Add explicit numbers before and after each implementation milestone:

- total RAMFS node count
- total RAMFS reserved file data bytes
- total RAMFS used file data bytes
- dirty directory count
- dirty file count
- open descriptor count
- process table slot count
- heap used/free/largest-free from `mmstat`
- boot-media buffer bytes
- block cache bytes
- largest single static FS scratch buffer

The first target should be visible in `mmstat` or a new `fsstat` command, not just inferred from source code.

For the 1 MB profile, `fsstat` must make it obvious whether System/1 is using a full-image floppy buffer. If it is, that configuration should be reported as not low-memory safe.

## Milestone 1: Add Filesystem RAM Accounting

**Goal:** Make RAM cost observable before changing behavior.

**Files:**
- Modify: `kernel/common/modules/fs/ramfs_core.h`
- Modify: `kernel/common/modules/fs/ramfs_core.c`
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/shell/shell_core.c`

**Tasks:**
1. Add `ramfs_stats_t`:
   - `node_cap`
   - `node_used`
   - `file_nodes`
   - `dir_nodes`
   - `reserved_file_bytes`
   - `used_file_bytes`
2. Add `fs_core_stats_t`:
   - RAMFS stats
   - dirty dir/file counts
   - boot media kind
3. Add shell command `fsstat`.
4. Keep `mmstat` unchanged.

**Verification:**

```bash
make modules-32
make modules-64
make modules-img-32
git diff --check
```

Manual:

```text
fsstat
touch test.txt
write test.txt ahoj
fsstat
```

Expected:
- dirty file count increases after `write`.
- used file bytes reflect written content.
- reserved file bytes show the current fixed-buffer cost.
- low-memory unsafe buffers are visible.

## Milestone 2: Remove Full-Floppy-Image Dependency

**Goal:** Floppy boot and write-back must not require a 1.44 MB image buffer in RAM.

**Current Problem:**

The current write-back path serializes changes into `g_boot_floppy_info->floppy_image_addr` and then writes sectors out. This is simple, but it cannot be the required path for a 1 MB machine because the floppy image alone is larger than total RAM.

**Design:**

Add a streaming FAT12 mutation backend that reads and writes sectors through `block_core`/FDC using tiny fixed buffers:

- one FAT sector buffer
- one directory sector buffer
- one data sector buffer
- optional second sector buffer for read-modify-write

The backend must support:

- creating a directory
- creating/truncating a file
- writing file content cluster by cluster
- freeing an old cluster chain when overwriting a file
- deleting a file in a later milestone

The full-image path may remain as a debug fast path when `floppy_image_addr` exists, but it must not be required. `fs_core_flush_to_boot_media()` should prefer streaming write-back in low-memory mode.

**Files:**
- Modify: `kernel/common/modules/fs/fat12_core.c`
- Modify: `kernel/common/modules/fs/fat12_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/fs/block_core.c`
- Modify: `kernel/common/modules/fs/block_core.h`

**Implementation Notes:**

- Add FAT12 helpers that operate on a `block_device_t*`, not just `uint8_t* image`.
- Keep FAT12 directory operations sector-scoped.
- Do not allocate arrays proportional to disk size.
- Do not scan the whole disk into RAM. Linear FAT/directory scans are acceptable for now because 1.44 MB floppy latency is acceptable for this OS.
- If the bootloader still provides a full image, use it only as an optional cache and report it in `fsstat`.

**Verification:**

```bash
make modules-img-32
make -B img-32
git diff --check
```

Manual floppy smoke:

```text
touch test.txt
write test.txt ahoj
cat test.txt
shutdown
```

Answer `Y`, reboot:

```text
cat test.txt
```

Expected:
- `test.txt` persists.
- `fsstat` reports no required 1.44 MB floppy image buffer in the low-memory path.

## Milestone 3: Stop Reserving File Data for Imported Placeholder Files

**Goal:** Imported read-only files should not reserve `RAMFS_FILE_CAP` bytes each.

**Current Problem:**

Every RAMFS file node has a static data buffer. Imported placeholder files do not need writable content until they are modified.

**Design:**

Split RAMFS file storage into metadata and optional data storage:

```c
typedef enum {
    RAMFS_STORAGE_NONE = 0,
    RAMFS_STORAGE_INLINE = 1
} ramfs_storage_kind_t;
```

Each file node records:
- storage kind
- size
- optional storage slot id

For the first implementation, keep a fixed pool of file-data slots:

```c
#define RAMFS_DATA_SLOT_CAP 4u
#define RAMFS_DATA_SLOT_SIZE 4096u
```

This is 16 KiB of writable file data in the low-memory profile. Larger builds can raise the cap, but the low-memory default must stay small. If all slots are in use, writes should fail with `FS_ERR_NO_SPACE` or a future `FS_ERR_NO_MEM`.

Imported files start with `RAMFS_STORAGE_NONE`.

On `FS_O_CREAT`, `FS_O_TRUNC`, `FS_O_WRONLY`, `FS_O_RDWR`, or first write:
- allocate a data slot,
- copy existing content from boot media if needed and possible,
- then apply the write.

**Files:**
- Modify: `kernel/common/modules/fs/ramfs_core.c`
- Modify: `kernel/common/modules/fs/ramfs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`

**Verification:**

Manual:

```text
fsstat
cat /Documents/file.txt
fsstat
write test.txt ahoj
cat test.txt
fsstat
```

Expected:
- reading an unchanged imported file does not allocate RAMFS data storage.
- writing a new file allocates one data slot.
- `cat test.txt` prints the written data before reboot.

## Milestone 4: Replace Dirty Path Lists With Node-Based Dirty Metadata

**Goal:** Stop storing full paths for every dirty item where a RAMFS node can carry dirty state.

**Current Problem:**

Dirty tracking stores full normalized paths in fixed arrays. This duplicates strings and can miss operations that mutate already-open files.

**Design:**

Add flags to RAMFS nodes:

```c
#define RAMFS_NODE_DIRTY_META 0x01u
#define RAMFS_NODE_DIRTY_DATA 0x02u
#define RAMFS_NODE_DELETED    0x04u
```

Write-back walks RAMFS and serializes dirty nodes. It reconstructs paths during traversal into one scratch path buffer instead of storing many full paths.

**Tasks:**
1. Mark new directories as `DIRTY_META`.
2. Mark created/truncated/written files as `DIRTY_META | DIRTY_DATA`.
3. Mark deleted files as `DELETED`.
4. Replace `g_dirty_dirs` and `g_dirty_files` with RAMFS traversal.
5. Make `rm` persistent for FAT12 by serializing deletes.

**Verification:**

```text
touch a.txt
write a.txt hello
rm a.txt
shutdown
```

After reboot:
- `a.txt` must not exist.

## Milestone 5: Lazy Directory Import

**Goal:** Do not import the whole directory tree at boot.

**Current Problem:**

Boot imports every visible directory and file name into RAMFS immediately. This front-loads RAM and boot time.

**Design:**

Import only `/` at boot. Mark directories as lazy. When `ls`, `cd`, `stat`, or `open` touches an unexpanded directory:
- read entries from boot media,
- create RAMFS child metadata nodes,
- mark directory as expanded.

Newly created RAMFS entries override boot-media entries.

**Required Behavior:**

- `ls /Documents` still works.
- autocomplete still works.
- unchanged file content still comes from boot media.
- dirty RAMFS entries are preferred over boot-media entries.

**Verification:**

```text
fsstat
ls /
fsstat
ls /Documents
fsstat
cat /Documents/file.txt
```

Expected:
- node count grows only when directories are visited.

## Milestone 6: Tiny Block/Read Cache

**Goal:** Avoid repeated boot-media reads while keeping cache memory bounded.

**Design:**

Add a tiny fixed block cache in `block_core` or `fs_core`:

- 2 to 8 cache entries
- sector/LBA key
- media kind/device key
- dirty flag reserved for later
- simple clock or round-robin eviction

For the 1 MB profile, the default should be 2 entries of 512 bytes each. Larger builds can use more entries. Start read-only. FAT12 write-back can invalidate affected entries after flushing.

**Verification:**

Add optional counters:
- block cache hits
- block cache misses
- evictions

Manual:

```text
cat /Documents/file.txt
cat /Documents/file.txt
fsstat
```

Expected:
- second read has cache hits.

## Milestone 7: Heap-Backed Variable File Storage

**Goal:** Stop wasting 4096 bytes for every dirty small file.

**Prerequisite:** MM heap must be stable enough for kernel allocation/free.

**Design:**

Replace fixed-size data slots with heap allocations:

- allocate exact or rounded-up file capacity
- grow on write
- shrink on truncation if useful
- free on unlink

Keep an upper bound per file to avoid exhausting RAM accidentally.

For the 1 MB profile:

- default maximum dirty file size should stay <= 4096 bytes unless explicitly configured higher
- total dirty file data should stay <= 16 KiB
- allocation failure must leave the old file contents intact

**Verification:**

```text
fsstat
write small.txt hi
fsstat
write big.txt <large test text>
fsstat
rm small.txt
fsstat
```

Expected:
- small file consumes much less than 4096 bytes.
- `rm` releases file data memory.

## Milestone 8: Optional Overlay-Like Node Model

**Goal:** Make the RAM layer store only changes, while unchanged metadata remains in the lower boot filesystem.

**Design:**

Introduce VFS lookup result with origin:

```c
typedef enum {
    VFS_ORIGIN_LOWER = 1,
    VFS_ORIGIN_UPPER = 2
} vfs_origin_t;
```

Directory listing merges:
- lower boot-media entries
- upper RAMFS entries
- whiteouts/deletes

This is closer to overlayfs, but still small enough for System/1.

**Do this only after:**
- node-based dirty tracking works
- lazy directory import works
- persistent delete works

## Implementation Order

1. Add RAM accounting and `fsstat`.
2. Remove the required full-floppy-image write-back path.
3. Remove fixed data buffers from imported placeholder files.
4. Move dirty tracking from path lists into RAMFS node flags.
5. Add persistent delete/write-back traversal.
6. Add lazy directory import.
7. Add a tiny bounded read cache.
8. Move dirty file data from fixed slots to heap-backed variable storage.
9. Consider overlay-like lower/upper VFS model.

## Risk Notes

- Lazy import can break autocomplete and `ls` if path normalization and lookup origin are not handled consistently.
- Dirty node traversal needs stable path reconstruction.
- FAT12 write-back has 8.3 name limits; long names require either rejection or long filename write support.
- Heap-backed storage must not fragment memory badly before process/user-mode work.
- Read cache invalidation must be explicit after write-back.
- A 1 MB machine cannot tolerate hidden full-media buffers; `floppy_image_addr` must be optional.
- Streaming FAT12 write-back is slower than image mutation, but it is the correct tradeoff for low RAM.
- Keeping too many open files/process slots can consume RAM even without user programs; cap low-memory builds conservatively.

## Low-Memory Configuration Defaults

Add central config macros later, but use these target defaults:

```c
#define SYSTEM1_LOW_RAM_TARGET          1
#define SYSTEM1_FS_BLOCK_CACHE_ENTRIES  2
#define SYSTEM1_RAMFS_DATA_SLOT_CAP     4
#define SYSTEM1_RAMFS_DATA_SLOT_SIZE    4096u
#define SYSTEM1_PROCESS_MAX_LOW         4u
#define SYSTEM1_POSIX_FD_CAP_LOW        12u
#define SYSTEM1_DIRTY_NODE_CAP_LOW      32u
```

Low-memory builds should prefer fewer processes and descriptors until scheduler/user programs exist. High-memory builds can override these caps.

## Final Verification

Build:

```bash
make modules-32
make modules-64
make modules-img-32
make iso-32
make iso-64
make img-32
git diff --check
```

Manual floppy smoke:

```text
fsstat
touch test.txt
write test.txt ahoj
cat test.txt
fsstat
shutdown
```

Answer `Y`, reboot:

```text
cat test.txt
fsstat
```

Expected:
- `cat test.txt` prints `ahoj` before and after reboot.
- RAM usage metrics are lower than the baseline after milestones 3 and 6.
- The floppy write-back path does not require a full 1.44 MB image buffer.
