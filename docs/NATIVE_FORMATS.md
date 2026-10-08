# System Native Binaries and Module Migration

Status: **incremental implementation**, not yet runtime loadable.

## Native formats

- **SKRN**: kernel image, `.bin` (planned custom header; current boot images remain ELF for compatibility).
- **SMOD**: loadable kernel module, `.mod` (planned custom relocatable format).
- **SPRG**: userspace program, `.prg` (existing custom format, unchanged).

ELF may be supported separately but is **not** the internal native binary format.

## Module boundary, phase 1

`src/kernel/core/tty/display.h` defines architecture-neutral
`system_display_ops_t`. TTY uses this interface and does not include
`vga.h`. The VGA adapter registers its operations in
`display_platform_init()` **before** `tty_init()` on current x86 targets.
The registered functions are called through a driver table.

**Important:** This is currently a built-in **bootstrap driver**.
It is still linked into the kernel: no on-disk `.mod` file is loaded yet.
It stays built-in until an early-boot module loader can read boot media safely.
Do not delete VGA sources or disable the VGA build module yet.

`tty.h` and TTY line editing stay in the kernel. Only the hardware-specific
display implementation is a driver. The future loader may register a
replacement display driver with the same ABI.

## Driver separation, phase 2

Hardware-specific PC VGA and PS/2 code is now located in
`src/kernel/drivers/pc/`, not `src/kernel/core/`.
The core TTY and POSIX descriptor layer use the generic `input.h` event
interface instead of `keyboard.h`. PC input and display drivers register
their operations before the TTY and IRQ paths start. The three x86 boot
targets include the PC drivers explicitly in the Makefile.

These drivers are still **statically linked for bootstrap**. This step
separates source and APIs; genuine runtime-loaded SMOD requires its own
format reader, symbol/relocation linker and boot media support.

## SMOD on-disk format design constraints

Implement this as an independent binary format (not renamed ELF):

- Four-byte ASCII magic `SMOD`, format revision and target CPU identifier.
- Explicit fixed-width little-endian fields; do not memcpy host C structs.
- Bounded sections for code/data/BSS, imports, exports, dependencies,
  relocations and entrypoints; validate bounds and integer overflow.
- Versioned **System Module ABI** independent of file format revision.
- Architecture-specific relocation definitions with a common container.
- Kernel-checked symbol allowlist; reject unknown symbols, wrong ABI and CPU.
- Reserve early-boot minimum drivers before mounting the real filesystem.
- Treat on-disk modules as trusted kernel code only after integrity checks;
  a malformed or untrusted module must never be executed.

The loader should first load a simple self-contained test module (no hardware),
then move a non-essential driver, and only then make VGA disk-loadable.

## Cross-machine portability

For machines with the same CPU ISA, the identical SKRN *may* be reused
if boot, memory-map and ABI conditions are compatible. A MOS 6510 C64
cannot execute i386 kernel machine code. It can share System/1 kernel
source/interfaces but needs a separately compiled image and a compact loader.

For FAT12 targets, `.mod` and `.bin` fit 8.3 names.


## Essential platform CPU/power abstraction

A separate `system_platform_ops_t` registry holds the basic idle,
halt/panic, reset, and poweroff operations. The native PC implementation
is in `src/kernel/drivers/pc/platform_pc.c`, and each current x86
kernel entry registers it before calling into the TTY.

Generic services (`klog`, TTY, POSIX, signals) do not execute PC-specific
`hlt`, `cli`, `inb` or `outb` instructions directly anymore. This
is an essential **built-in boot platform ABI**; it cannot be replaced by a
late SMOD module for early panic and reset operations. Hardware/device
drivers continue to use `.mod` once the disk is mounted.
