# POSIX Processes and Exec Design

## Goal

Define the next architecture step after the POSIX file, descriptor, syscall, and user ABI layers. This design introduces process boundaries, per-process kernel state, user address spaces, and an `execve` path without implementing them in this milestone.

The first implementation should still be conservative: one runnable process can be enough. The design must make later multiple processes, scheduling, and `fork` possible without forcing them into the first process milestone.

## Non-Goals

- No `fork` implementation in the first process milestone.
- No preemptive scheduler requirement.
- No signals, users, groups, pipes, or permissions.
- No dynamic linker.
- No general-purpose virtual memory policy beyond what `execve` needs.
- No requirement to load programs from ISO media as writable files.
- No direct ELF execution requirement. ELF can remain an intermediate toolchain format, but the kernel loader should consume System/1 `.prg` files.

## Process Table

Add a fixed-size kernel process table, for example `PROCESS_MAX 16`, owned by a future `process` module. Each entry should contain:

- `pid`: small positive integer, never reused while the slot is active.
- `state`: unused, runnable, running, blocked, zombie.
- `exit_status`: meaningful for zombie/completed processes later.
- `cwd`: normalized absolute path copied from the parent or initial kernel process.
- `fd_table`: per-process file descriptor table.
- `address_space`: architecture-specific page directory/table root.
- `entry_ip` and `user_sp`: initial instruction pointer and user stack pointer for user mode entry.
- `kernel_stack`: reserved kernel stack for trap/syscall handling.
- `name`: short diagnostic program name.

The initial shell can remain kernel-resident while the process table is introduced. The first process slot should represent the kernel shell context so future syscalls always have a current process.

## Current Process

Introduce `process_current()` after the table exists. Syscall dispatch should use current-process state instead of global state for:

- file descriptors
- current working directory
- address-space validation

Early boot can create one kernel process after `fs_init()`, `mm_init()`, and `syscall_init()`. Until user mode entry exists, this current process can run on the existing kernel stack and share kernel address space.

## File Descriptors

Move the fixed descriptor table from global POSIX state into each process entry. Keep descriptor behavior unchanged:

- fd `0`: TTY input
- fd `1`: TTY output
- fd `2`: TTY output
- fd `3+`: VFS nodes

The POSIX wrapper functions should look up `process_current()->fd_table`. Descriptor close-on-exec can be deferred, but the table layout should leave room for per-fd flags.

For `execve`, descriptors remain open by default. This matches the simplest POSIX behavior and makes shell-launched programs immediately usable with inherited stdin/stdout/stderr.

## Current Working Directory

Move cwd ownership out of filesystem driver globals and into the process table.

The FS layer should eventually accept cwd as an input to path normalization rather than reading one global cwd. Transitional APIs can keep `fs_get_cwd_path()` and `fs_change_dir()` as wrappers over the current process:

- `fs_get_cwd_path()` returns `process_current()->cwd`.
- `fs_change_dir(path)` validates the target through VFS, then updates `process_current()->cwd`.

This prevents one future process from changing another process's cwd.

## Address Spaces

Keep the kernel mapped in every address space. User processes get a separate user range containing:

- executable code and read-only data
- writable data/BSS
- user stack
- optional guard page below stack

The first implementation can use identity or simple fixed virtual mappings if the existing paging layer cannot yet allocate arbitrary user regions. The important boundary is architectural: a process owns an address-space object, and `execve` replaces that object atomically after the program image is validated.

Syscall pointer arguments must eventually be validated against the current process user range before dereference. Until user mode exists, dispatcher validation can remain a TODO, but the syscall boundary should not grow APIs that assume trusted kernel pointers forever.

## SPRG Program Format

System/1 programs use the `.prg` extension and start with the four byte magic:

```text
SPRG
```

`SPRG` is a small ELF-like executable format, not a raw flat binary. It keeps the useful `PT_LOAD` segment idea while removing sections, relocations, interpreters, dynamic linking, and symbol tables from the kernel loader.

The first version should use little-endian 32-bit fields:

```c
typedef struct {
    char magic[4];        /* "SPRG" */
    uint32_t version;     /* 1 */
    uint32_t arch;        /* 1 = i386, 2 = x86_64 */
    uint32_t entry;       /* virtual entry point */
    uint32_t phoff;       /* program header table offset */
    uint32_t phnum;       /* program header count */
    uint32_t flags;       /* reserved, must be 0 for v1 */
} sprg_header_t;

typedef struct {
    uint32_t type;        /* 1 = LOAD */
    uint32_t offset;      /* file offset */
    uint32_t vaddr;       /* target virtual address */
    uint32_t filesz;      /* bytes copied from file */
    uint32_t memsz;       /* bytes mapped/zeroed */
    uint32_t flags;       /* bit 0 R, bit 1 W, bit 2 X */
    uint32_t align;       /* expected page alignment */
    uint32_t reserved;    /* must be 0 for v1 */
} sprg_phdr_t;
```

The kernel should reject a program unless:

- the path ends in `.prg`
- the first four bytes are exactly `SPRG`
- `version == 1`
- `arch` matches the boot target
- all program headers fit inside the file
- every program header is type `LOAD`
- `memsz >= filesz`
- segment ranges do not overlap

ELF can still be used as a compiler/linker intermediate. A linker script or later converter should produce the final `.prg` layout with an `SPRG` header at offset zero.

## SPRG Loader Requirements

Add a future `sprg` or `loader` module that can load static System/1 `.prg` executables from VFS files. Initial requirements:

- Validate `.prg` extension, `SPRG` magic, version, architecture, and header bounds.
- Support only the architecture being booted.
- Load `SPRG_LOAD` segments.
- Respect segment permissions at least as metadata; strict page permissions can follow paging support.
- Zero BSS where `memsz > filesz`.
- Reject overlapping, malformed, or out-of-range segments.
- Return entry point, initial user stack pointer, and address-space metadata.

The first supported executable format should be statically linked, no interpreter, no dynamic relocations.

## `execve` ABI

Add syscall number later, likely:

```c
#define SYS_EXECVE 59u
```

Kernel signature:

```c
int posix_execve(const char* path, char* const argv[], char* const envp[]);
```

Initial behavior:

- Resolve `path` relative to current process cwd.
- Reject paths that do not end in `.prg`.
- Open and validate the executable.
- Build a new address space.
- Copy argument and environment strings onto the new user stack.
- Preserve open file descriptors.
- Preserve cwd.
- Replace current process image.
- On success, do not return to the old image.
- On failure, leave the old process image intact and return negative errno.

Environment support can start minimal. Passing `envp == 0` should mean an empty environment.

## User Stack Layout

Use a simple C ABI stack layout:

- argument strings and environment strings copied near the top of stack
- pointer arrays for `argv` and `envp`
- `argc`
- aligned initial stack pointer

Exact ordering should be documented in the implementation plan for each architecture. The first implementation can target i386 first and add x86_64 after the convention is verified.

## `fork` Deferral

Defer `fork` until after:

- per-process descriptor tables exist
- cwd is per-process
- address-space cloning or copy-on-write policy exists
- kernel stacks and trap return paths are stable
- there is at least cooperative process switching or a scheduler plan

Until then, support process creation through `execve` from a kernel shell command or a later `spawn`/`posix_spawn`-style helper. This avoids pretending the kernel can clone execution state before it has the memory and scheduler machinery to do it correctly.

## Implementation Sequence

1. Add process table and current-process API.
2. Move fd table into process state while preserving existing shell behavior.
3. Move cwd into process state and adapt FS path normalization wrappers.
4. Add syscall pointer-validation hooks, initially permissive for kernel callers.
5. Add SPRG loader for static `.prg` executables.
6. Add `execve` syscall and kernel-side smoke command.
7. Add user-mode transition for one architecture.
8. Repeat for remaining boot targets.

## Verification Strategy

Before user mode:

- Existing shell commands still pass module and image builds.
- fd inheritance can be tested through kernel-side process table tests or diagnostic commands.
- cwd isolation can be tested by switching current-process slots in kernel diagnostics.

After user mode:

- Load a minimal program that writes to fd `1`.
- Run a program with argv and verify it prints arguments.
- Run a program that opens, reads, writes, stats, and unlinks files through syscall wrappers.
- Verify failed `execve` leaves the caller alive.
