# System/1 Native Modules — SMOD v1

SMOD is the System/1 native module container, **not ELF**. Source and object
files may be compiled with GCC/binutils, but the installed `.mod` binary
contains a System-defined header and executable bytes.

## Binary header (32 bytes, little endian)

| Offset | Field | Bytes |
|---|---|---|
| 0 | Magic = `SMOD` | 4 |
| 4 | Format version = 1 | 2 |
| 6 | System Module API version = 1 | 2 |
| 8 | CPU architecture: 1=i386, 2=x86_64 | 2 |
| 10 | Flags = 1 (executable) | 2 |
| 12 | Header/image offset = 32 | 4 |
| 16 | Image size, max 4096 | 4 |
| 20 | Memory size (including BSS), max 4096 | 4 |
| 24 | Entry offset, strictly within image | 4 |
| 28 | Reserved = 0 | 4 |

The entrypoint is `int entry(const smod_api_v1_t*)`; the first API
revision offers an ABI identifier and a kernel callback. The loader performs
strict header, file length and architecture checks, copies the program into
kernel-managed memory, clears BSS, calls the entry and retains successful
images. It loads files from `/boot/modules/*.mod` after the real filesystem
is mounted. Invalid/unsupported modules are skipped without preventing boot.

This **initial executable subset** is deliberately limited to
self-contained position-independent images with **no unresolved relocations**.
The compiler's ELF object files are only a build-time intermediate. Before
shipping VGA/PS2 drivers as SMOD, implement relocations, import/export
resolution, dependency order, integrity policy and a safe bootstrap path.
Built-in PC console and keyboard drivers remain necessary for early boot.

## Build

```sh
make smod-32 smod-64 smod-check
make iso-32 iso-64 img-32
```

The initial test module was replaced in boot images by real PC hardware
modules (`com1.mod` and `cmos.mod`). The loader is common code under
`src/kernel/core/module/`. On C64 or MULTIPLAN/1 use this same header
with a different CPU architecture ID and corresponding architecture-specific
code generator/entry calling convention, **not an x86 binary**.


## Driver isolation: PC interrupt controllers

System/1 generic interrupt dispatch now uses the `system_irq_chip_ops_t`
registry. Hardware-specific 8259 PIC / 8253 PIT I/O lives in
`src/kernel/drivers/pc/pic_pit.c`, not `interrupts_common.c`.
The current x86 boot path registers this driver before initializing
interrupts. TTY, POSIX, scheduling and generic IRQ dispatch remain kernel
services. Runtime replacement of the interrupt controller is **not**
supported yet; the initial driver is a built-in bootstrap driver.


## 1 MiB floppy bootstrap memory

The floppy boot profile has only 1 MiB of installed physical RAM. The
current MM fallback marks memory beginning at 1 MiB as available; it must
not be used to allocate executable boot modules on this profile. The SMOD
loader therefore uses a **single statically reserved 4096-byte resident
arena** for the first module and refuses additional resident modules rather
than allocating nonexistent physical memory. This is a compatibility measure
until the floppy physical-memory map is provided to the page allocator.
The full i386/x86_64 boot profiles continue to use `kmalloc`.

## First real dynamically loaded PC drivers (ABI revision 2)

Native modules are now compiled as freestanding position-independent code
and linked at image address zero with `tools/linker/linker.smod.ld` (a
temporary **build-time** ELF; installed `.mod` files are fully native).

- `com1.mod`: probes and initializes the 16550 COM1 UART at 0x3F8
  (38400 baud, 8N1) and registers a kernel console serial mirror.
  On machines without a working compatible UART the module is skipped.
- `cmos.mod`: reads PC AT CMOS RTC with BCD/binary and 12/24-hour handling,
  registers a clock callback and logs the actual clock value at boot.

The System Module API has been extended from revision 1 to **revision 2**,
retaining the 32-byte container header (format revision 1). The callbacks
are installed only after a successful module entrypoint. Boot includes
`/boot/modules/com1.mod` and `/boot/modules/cmos.mod`. The 1 MiB floppy
uses a bounded 4 KiB **shared resident arena** so both modules fit if their
combined aligned images fit.

TTY and the early VGA/PS2 console remain built-in: they are needed before
boot media can be mounted. This implementation loads actual independent PC
peripheral code, rather than wrapping existing kernel hardware functions.
VGA and PS/2 can be moved later after a suitable hot-swap procedure, IRQ
handoff, and an extended SMOD image format for larger code/data.

## Packaging

`SMOD_DRIVERS` in the Makefile specifies the system's default native driver
modules. ISO and floppy packaging copy only the selected module files,
so stale artifacts such as the earlier demo `hello.mod` are never silently
included by a wildcard. System Module API v1 objects are rejected by the
new ABI v2 loader.

## Platform isolation: floppy disk controller

The PC floppy disk controller (FDC), channel-2 ISA DMA programming,
seek/recalibration and sector commands now live in
`src/kernel/drivers/pc/floppy_fdc.c`. The hardware-neutral
`floppy_controller.h` interface lives in `src/kernel/core/fs/`.
The common filesystem layer retains cached FAT/boot sector handling,
VFS and generic block device logic, and **does not use x86 I/O ports**.

The i386 floppy platform explicitly registers the PC FDC at boot, before
the physical filesystem is mounted. This driver must currently be built
into the **bootstrap kernel**: a module stored on the disk cannot be
used to read the disk that contains it. Future platforms can register
different controller ops and still reuse the FAT12 and VFS code.


## Early SMOD boot stage (before the filesystem)

SMOD now has two stages. The first is called directly from the architecture
boot entry, **after the minimal paging map is ready** (x86_64 starts with only
2 MiB mapped), but **before the heap allocator, IRQ controller, filesystem or
userspace**. The validated module images are copied into a dedicated, bounded
4 KiB resident arena in the kernel and activated through the same SMOD ABI.
They cannot allocate with `kmalloc` or rely on POSIX at this stage.

- **GRUB / Multiboot2**: the loader accepts only module tags whose
  command line starts with `smod:`. Their code ranges and tag bounds
  must be within the active identity mapping. `rootfs.iso` is excluded.
  The GRUB menu preloads `com1.mod` and `cmos.mod` as separate modules.
- **i386 floppy**: the same native `.mod` binaries are embedded by
  `early_smod.S` into the boot payload and validated / executed by the
  early loader. This removes the cycle of having to read the disk before
  the disk controller exists. The source .mod files also remain on FAT12.

After the real filesystem is mounted, the ordinary second-stage loader
scans `/boot/modules`. Names of already activated early modules are
remembered and skipped; only additional modules are loaded.

**Important limitation**: this is not yet a fully dynamic microkernel.
The essential boot stub, page mapping, kernel TTY, loader, and the initial
FDC/VGA/PS2 bootstrap backends are still statically linked. Moving them
requires preboot access to hardware, new driver APIs, and (for VGA/PS2)
full native code/data/relocation support. They cannot all be loaded from
the filesystem they are needed to mount.

## PC native hardware module set

Boot media now distributes six native drivers:
`com1.mod` (UART), `cmos.mod` (RTC), `picpit.mod` (8259 PIC and
8253 PIT), `vga.mod` (VGA console with cursor/line editing),
`ps2.mod` (PS/2 keyboard and scancode translation) and `fdc.mod`
(82077 floppy controller with 8237 DMA programming).

The SMOD API revision 3 registers typed device operation tables. The
kernel **copies the operation tables** and activates a new driver only
after a successful entrypoint; callbacks remain in resident module code.
The entire module image including zero-initialized state must fit the
native SMOD memory limit. The floppy image also retains the built-in
hardware fallback adapters: they are required when a module fails
validation or when the small floppy early-module arena fills up. The
generic TTY, VFS, POSIX, CPU paging, and usermode services remain core
kernel facilities, not magically hot-swappable modules.

Removing all built-in boot backends safely also requires a real
pre-kernel device discovery/driver manager and a memory map compatible
with the floppy's 1 MiB constraint.
