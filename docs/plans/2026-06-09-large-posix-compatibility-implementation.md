# Large POSIX Compatibility Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Turn System/1 from a shell-oriented kernel into a small POSIX-like environment with stable file descriptors, errno-style errors, POSIX command behavior, and a syscall boundary that can later support user processes.

**Architecture:** Keep the current modules and boot targets intact. First normalize kernel contracts around POSIX names, paths, file handles, and errors; then add a syscall/libc boundary; then migrate shell commands to the same interfaces user programs will use. Process creation and `exec` come last because they depend on paging, file descriptors, and a stable ABI.

**Tech Stack:** Freestanding C, existing `kernel/common/modules/*` layout, current `fs`/`vfs`/`ramfs`/`tty`/`mm` modules, i386 and x86_64 module builds, QEMU smoke testing for ISO and floppy images.

---

## Scope

This is a multi-milestone plan. Do not try to implement everything in one commit.

POSIX-like means:
- Commands follow standard behavior where practical: `cat` writes bytes, `echo` formatting is predictable, `cd` with no arg can later use `$HOME`, and errors use familiar wording.
- Filesystem paths support `.` and `..`, absolute and relative paths, repeated slashes, and stable normalized paths.
- Kernel internals expose POSIX-shaped primitives: `open`, `close`, `read`, `write`, `lseek`, `stat`, `mkdir`, `chdir`, `getcwd`, `unlink` when the FS supports it.
- File descriptors `0`, `1`, and `2` exist and map to TTY stdin/stdout/stderr.
- A syscall ABI exists before user processes, so the shell can be migrated onto the same contracts.

Out of scope for the first implementation pass:
- Full Unix permissions, users, groups, signals, pipes, fork/exec, ELF loading, and preemptive scheduling.
- Persistent write-back for every POSIX mutation on ISO media.
- Full POSIX test-suite compliance.

## Current Starting Point

Relevant existing files:
- `kernel/common/modules/fs/fs_core.h`
- `kernel/common/modules/fs/fs_core.c`
- `kernel/common/modules/fs/vfs_core.h`
- `kernel/common/modules/fs/ramfs_core.h`
- `kernel/common/modules/fs/ramfs_core.c`
- `kernel/common/modules/fs/fat12_core.c`
- `kernel/common/modules/fs/iso9660_core.c`
- `kernel/common/modules/shell/shell_core.c`
- `kernel/common/modules/tty/tty_core.h`
- `kernel/common/modules/tty/tty_core.c`
- `kernel/common/modules/mm/mm_core.h`
- `kernel/common/modules/mm/mm_core.c`
- `Makefile`

Existing verification commands:
- `make modules-32`
- `make modules-64`
- `make modules-img-32`
- `make iso-32`
- `make iso-64`
- `make img-32`
- `git diff --check`

---

### Task 1: Add POSIX Error Names Without Breaking Current FS API

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/shell/shell_core.c`
- Create: `kernel/common/modules/posix/posix_errno.h`
- Create: `kernel/common/modules/posix/posix.c`
- Create: `kernel/i386/modules/posix/posix.h`
- Create: `kernel/x86_64/modules/posix/posix.h`
- Create: `kernel/i386-floppy/modules/posix/posix.h`

**Step 1: Create the POSIX errno header**

Add negative kernel return values and positive errno constants:

```c
#ifndef SYSTEM1_COMMON_POSIX_ERRNO_H
#define SYSTEM1_COMMON_POSIX_ERRNO_H

#define POSIX_EPERM   1
#define POSIX_ENOENT  2
#define POSIX_EIO     5
#define POSIX_EBADF   9
#define POSIX_EACCES  13
#define POSIX_EEXIST  17
#define POSIX_ENOTDIR 20
#define POSIX_EISDIR  21
#define POSIX_EINVAL  22
#define POSIX_ENOSPC  28
#define POSIX_EROFS   30

#define POSIX_OK 0

#endif
```

**Step 2: Add FS-to-errno mapping**

Add `int fs_core_to_errno(int rc);` to `fs_core.h`.

Expected mapping:
- `FS_OK` -> `0`
- `FS_ERR_NOT_FOUND` -> `POSIX_ENOENT`
- `FS_ERR_EXISTS` -> `POSIX_EEXIST`
- `FS_ERR_NOT_DIR` -> `POSIX_ENOTDIR`
- `FS_ERR_INVALID` -> `POSIX_EINVAL`
- `FS_ERR_NO_SPACE` -> `POSIX_ENOSPC`
- `FS_ERR_READ_ONLY` -> `POSIX_EROFS`

**Step 3: Keep shell output stable**

Update `shell_print_fs_error()` to use the mapping internally but keep familiar output for now.

**Step 4: Verify**

Run:

```bash
make modules-32
make modules-64
make modules-img-32
git diff --check
```

Expected: all commands pass.

---

### Task 2: Normalize Paths With POSIX Semantics

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/fs/ramfs_core.c`

**Step 1: Extract path normalization**

Add:

```c
int fs_core_normalize_path(const char* cwd, const char* path, char* out, uint32_t out_cap);
```

Rules:
- Empty path is invalid.
- Absolute path starts at `/`.
- Relative path starts at `cwd`.
- Multiple slashes collapse to one slash.
- `.` is ignored.
- `..` removes the previous component but never climbs above `/`.
- Result always starts with `/`.
- Result never ends with `/` unless it is exactly `/`.

**Step 2: Replace local mutation-only normalization**

Use `fs_core_normalize_path()` in:
- `fs_core_change_dir()`
- `fs_core_make_dir()`
- `fs_core_list_dir()`
- `fs_core_read_file()`
- write-back dirty directory recording

**Step 3: Verify edge cases manually in QEMU**

Build and boot:

```bash
make iso-32
make img-32
```

Manual shell checks:

```text
pwd
cd /
cd ./Documents
pwd
cd ../Documents/../Documents
pwd
ls //Documents
cat ./file.txt
mkdir ./NewDir
ls .
```

Expected:
- No malformed paths.
- `pwd` prints normalized absolute paths.
- `ls .` and repeated slashes work.

---

### Task 3: Add POSIX-Like VFS File Operations

**Files:**
- Modify: `kernel/common/modules/fs/vfs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/fs_core.c`
- Modify: `kernel/common/modules/fs/ramfs_core.h`
- Modify: `kernel/common/modules/fs/ramfs_core.c`
- Modify: `kernel/common/modules/fs/fat12_core.c`
- Modify: `kernel/common/modules/fs/iso9660_core.c`

**Step 1: Add open flags and seek constants**

In `fs_core.h` add:

```c
#define FS_O_RDONLY 0x0001u
#define FS_O_WRONLY 0x0002u
#define FS_O_RDWR   0x0003u
#define FS_O_CREAT  0x0100u
#define FS_O_TRUNC  0x0200u
#define FS_O_APPEND 0x0400u

#define FS_SEEK_SET 0u
#define FS_SEEK_CUR 1u
#define FS_SEEK_END 2u
```

**Step 2: Extend `vfs_driver_t`**

Add optional callbacks:

```c
int (*open)(const char* path, uint32_t flags, uint32_t* out_node_id);
int (*read)(uint32_t node_id, uint32_t offset, char* buffer, uint32_t cap, uint32_t* out_size);
int (*write)(uint32_t node_id, uint32_t offset, const char* buffer, uint32_t size, uint32_t* out_written);
int (*size)(uint32_t node_id, uint32_t* out_size);
int (*close)(uint32_t node_id);
```

Keep old `read_file()` temporarily so the migration can be incremental.

**Step 3: Implement read-only open/read for media drivers**

For FAT12 and ISO9660:
- `open(path, FS_O_RDONLY, &node_id)` resolves a file.
- `read(node_id, offset, buffer, cap, out_size)` reads from that file.
- writes return `FS_ERR_READ_ONLY`.

**Step 4: Implement read/write open for RAMFS**

For RAMFS:
- `FS_O_CREAT` creates a regular file if missing.
- `FS_O_TRUNC` clears existing file content.
- `FS_O_APPEND` makes the descriptor layer start at end of file.
- Write marks RAMFS dirty.

**Step 5: Verify**

Run:

```bash
make modules-32
make modules-64
make modules-img-32
```

Expected: all module builds pass while `read_file()` still works.

---

### Task 4: Introduce Kernel File Descriptor Table

**Files:**
- Create: `kernel/common/modules/posix/fd_core.h`
- Create: `kernel/common/modules/posix/fd_core.c`
- Modify: `kernel/common/modules/posix/posix.c`
- Modify: `kernel/common/modules/tty/tty_core.h`
- Modify: `kernel/common/modules/tty/tty_core.c`

**Step 1: Create fixed descriptor table**

Start with a small static table:

```c
#define POSIX_FD_CAP 32u
#define POSIX_STDIN_FILENO 0
#define POSIX_STDOUT_FILENO 1
#define POSIX_STDERR_FILENO 2
```

Descriptor kinds:
- unused
- TTY input
- TTY output
- VFS file

**Step 2: Reserve standard descriptors**

During `posix_init()`:
- fd `0` maps to TTY input.
- fd `1` maps to TTY output.
- fd `2` maps to TTY output.

**Step 3: Add descriptor operations**

Add:

```c
int posix_open(const char* path, uint32_t flags);
int posix_close(int fd);
int posix_read(int fd, void* buffer, uint32_t count);
int posix_write(int fd, const void* buffer, uint32_t count);
int posix_lseek(int fd, int32_t offset, uint32_t whence);
```

Return convention:
- Success returns count, fd number, or new offset.
- Failure returns negative errno, for example `-POSIX_EBADF`.

**Step 4: Implement TTY write path**

`posix_write(1, data, count)` and `posix_write(2, data, count)` call `tty_putc()` byte by byte.

**Step 5: Verify**

Add a temporary internal smoke path behind a non-user shell command only if needed. Prefer testing through migrated `cat` in the next task.

Run:

```bash
make modules-32
make modules-64
make modules-img-32
```

Expected: all pass.

---

### Task 5: Migrate `cat` to POSIX Descriptor Semantics

**Files:**
- Modify: `kernel/common/modules/shell/shell_core.c`
- Modify: `kernel/common/modules/posix/posix.c`
- Modify: `kernel/common/modules/posix/fd_core.c`

**Step 1: Update `cat` behavior**

Make `cat`:
- Use `posix_open(path, FS_O_RDONLY)`.
- Loop with `posix_read(fd, buffer, sizeof(buffer))`.
- Write bytes using `posix_write(1, buffer, size)`.
- Close the fd.
- Do not add a newline that is not present in the file.
- Do not expand tabs in `cat`; tab rendering belongs in TTY output if wanted.

**Step 2: Support multiple files**

POSIX-like `cat a b c` concatenates all files in order.

**Step 3: Update errors**

Use command-specific messages:

```text
cat: path: No such file or directory
cat: path: Is a directory
cat: path: Bad file descriptor
```

**Step 4: Verify manually**

In QEMU shell:

```text
cat file.txt
cat file.txt file.md
cat no-such-file
cat Documents
```

Expected:
- Existing bytes are printed unchanged.
- Multiple files concatenate.
- No automatic trailing newline.

---

### Task 6: Move Tab Rendering to TTY Instead of `cat`

**Files:**
- Modify: `kernel/common/modules/tty/tty_core.h`
- Modify: `kernel/common/modules/tty/tty_core.c`
- Modify: `kernel/common/modules/shell/shell_core.c`

**Step 1: Add terminal tab stop behavior**

In `tty_putc('\t')`, expand to spaces until the next tab stop.

Use POSIX terminal convention:
- Default tab stop is every 8 columns.
- Expansion depends on current TTY cursor column.

**Step 2: Remove `shell_cat_putc()`**

`cat` should write bytes through `posix_write()` and not know about tabs.

**Step 3: Verify**

Manual check:

```text
cat file.txt
echo a	b
```

Expected:
- Tabs align on 8-column terminal tab stops.
- `cat` implementation has no tab-specific code.

---

### Task 7: Add POSIX-Like `stat`, Directory Metadata, and File Types

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/vfs_core.h`
- Modify: `kernel/common/modules/fs/ramfs_core.c`
- Modify: `kernel/common/modules/fs/fat12_core.c`
- Modify: `kernel/common/modules/fs/iso9660_core.c`
- Modify: `kernel/common/modules/posix/posix.c`
- Modify: `kernel/common/modules/shell/shell_core.c`

**Step 1: Add stat structure**

Add:

```c
#define FS_MODE_DIR  0040000u
#define FS_MODE_FILE 0100000u

typedef struct {
    uint32_t mode;
    uint32_t size;
} fs_stat_t;
```

**Step 2: Add `stat` callbacks**

Extend `vfs_driver_t`:

```c
int (*stat)(const char* path, fs_stat_t* out_stat);
```

**Step 3: Add POSIX wrappers**

Add:

```c
int posix_stat(const char* path, fs_stat_t* out_stat);
int posix_fstat(int fd, fs_stat_t* out_stat);
```

**Step 4: Update shell**

Add simple `stat <path>` command for diagnostics.

**Step 5: Verify**

Manual shell checks:

```text
stat /
stat file.txt
stat Documents
stat no-such-file
```

Expected:
- Directories and files report different modes.
- File size is visible.
- Missing files return `No such file or directory`.

---

### Task 8: Add Basic Writable File Commands

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.h`
- Modify: `kernel/common/modules/fs/vfs_core.h`
- Modify: `kernel/common/modules/fs/ramfs_core.c`
- Modify: `kernel/common/modules/posix/posix.c`
- Modify: `kernel/common/modules/shell/shell_core.c`

**Step 1: Add `touch`**

Shell behavior:
- `touch file` creates an empty file if missing.
- Existing file remains unchanged.
- On ISO write-back, RAMFS can mutate in memory, but shutdown reports read-only media if changes cannot persist.

**Step 2: Add simple redirection-free write command**

Add `write <file> <text...>` as a temporary development command:
- Open with `FS_O_CREAT | FS_O_TRUNC | FS_O_WRONLY`.
- Write joined arguments separated by spaces.
- Append newline.

This is not POSIX shell redirection yet; it exists to test write paths.

**Step 3: Add `rm` only for RAMFS**

Add VFS `unlink(path)`:
- Remove regular file.
- Return `FS_ERR_INVALID` or new `FS_ERR_IS_DIR` for directories.
- Mark RAMFS dirty.
- FAT12/ISO return read-only.

**Step 4: Verify**

Manual shell checks:

```text
touch new.txt
stat new.txt
write new.txt hello world
cat new.txt
rm new.txt
ls .
cat new.txt
```

Expected:
- File can be created, written, read, removed.
- Removed file returns not found.

---

### Task 9: Create Syscall ABI Skeleton

**Files:**
- Create: `kernel/common/modules/syscall/syscall_core.h`
- Create: `kernel/common/modules/syscall/syscall_core.c`
- Create: `kernel/common/modules/syscall/syscall.c`
- Create: `kernel/i386/modules/syscall/syscall.h`
- Create: `kernel/x86_64/modules/syscall/syscall.h`
- Create: `kernel/i386-floppy/modules/syscall/syscall.h`
- Modify: `kernel/i386/kernel.c`
- Modify: `kernel/x86_64/kernel.c`
- Modify: `kernel/i386-floppy/kernel.c`
- Modify: `Makefile`

**Step 1: Add syscall numbers**

Start with:

```c
#define SYS_READ   0u
#define SYS_WRITE  1u
#define SYS_OPEN   2u
#define SYS_CLOSE  3u
#define SYS_LSEEK  8u
#define SYS_STAT   9u
#define SYS_GETCWD 10u
#define SYS_CHDIR  11u
#define SYS_MKDIR  12u
#define SYS_UNLINK 13u
```

**Step 2: Add dispatcher**

Add:

```c
int32_t syscall_dispatch(uint32_t nr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
```

Map calls to `posix_*` functions.

**Step 3: Initialize syscall module**

Add `syscall_init()` during kernel boot after `tty_init()`, `fs_init()`, and `mm_init()`.

**Step 4: Avoid assembly trap entry for now**

The first pass only creates the dispatcher. Interrupt/trap entry can be added after this compiles and shell migration is stable.

**Step 5: Verify**

Run:

```bash
make modules-32
make modules-64
make modules-img-32
make iso-32
make iso-64
make img-32
```

Expected: all pass.

---

### Task 10: Add Minimal libc-Style User ABI Headers

**Files:**
- Create: `include/system1/unistd.h`
- Create: `include/system1/fcntl.h`
- Create: `include/system1/errno.h`
- Create: `include/system1/stat.h`
- Create: `lib/user/syscall.c`
- Create: `lib/user/unistd.c`

**Step 1: Add public constants**

Mirror the kernel syscall and flag numbers.

**Step 2: Add wrappers**

Add wrappers:

```c
int open(const char* path, int flags);
int close(int fd);
int read(int fd, void* buf, unsigned count);
int write(int fd, const void* buf, unsigned count);
int chdir(const char* path);
int mkdir(const char* path);
```

For now wrappers can call a C stub that is used only by tests or kernel-side experiments.

**Step 3: Verify**

Add build target only if user programs are introduced. Otherwise run:

```bash
make modules-32
make modules-64
make modules-img-32
git diff --check
```

Expected: kernel builds still pass.

---

### Task 11: Improve Shell Parsing Toward POSIX Utility Behavior

**Files:**
- Modify: `kernel/common/modules/shell/shell_core.c`

**Step 1: Add quoted argument parsing**

Support:
- `echo "hello world"`
- `echo 'hello world'`
- escaped quotes inside double quotes if reasonable

Do not implement full shell grammar yet.

**Step 2: Add `--` option terminator support where useful**

Apply to commands that parse options later.

**Step 3: Make `echo` predictable**

Support:
- `echo text`
- `echo -n text`

Avoid non-portable escape behavior for now.

**Step 4: Verify**

Manual shell checks:

```text
echo hello world
echo "hello world"
echo -n no-newline
cat file.txt
```

Expected:
- Quoted text remains one argument.
- `echo -n` omits newline.

---

### Task 12: Add Process and Program Loading Design Checkpoint

**Files:**
- Create: `docs/plans/2026-06-09-posix-processes-and-exec-design.md`

**Step 1: Write the design before implementation**

Cover:
- process table layout
- per-process file descriptor tables
- current working directory per process
- kernel vs user address spaces
- ELF loader requirements
- `execve` ABI
- where `fork` is deferred

**Step 2: Do not implement processes in this task**

Process support has too much blast radius. It needs a separate plan after the file descriptor and syscall layers work.

## Final Verification

Run all build checks:

```bash
make modules-32
make modules-64
make modules-img-32
make iso-32
make iso-64
make img-32
git diff --check
```

Boot smoke tests:

```bash
qemu-system-i386 -cdrom build/artifacts/images/system1-iso-32.iso
qemu-system-x86_64 -cdrom build/artifacts/images/system1-iso-x86_64.iso
qemu-system-i386 -fda build/artifacts/images/system1-img-32.img
```

Manual smoke script for each boot target:

```text
pwd
ls /
cd /Documents
pwd
cd ..
cat /Documents/file.txt
stat /Documents/file.txt
touch /tmp.txt
write /tmp.txt hello posix
cat /tmp.txt
rm /tmp.txt
shutdown
```

Expected:
- All three boot targets reach shell.
- Standard commands work through descriptor/POSIX wrappers.
- `cat` preserves file bytes.
- TTY renders tabs consistently.
- Writable RAMFS changes behave consistently, and read-only media errors are explicit.

## Implementation Order

Recommended order:
1. Task 1 and Task 2: low-risk compatibility cleanup.
2. Task 3 and Task 4: core POSIX primitives.
3. Task 5 and Task 6: migrate one real command and fix terminal behavior.
4. Task 7 and Task 8: metadata and writable file commands.
5. Task 9 and Task 10: syscall/libc ABI.
6. Task 11: shell compatibility.
7. Task 12: design checkpoint for processes.

Do not start Task 9 until `cat` works through file descriptors on all boot targets.
