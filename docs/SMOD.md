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
shipping real VGA/PS2 drivers as SMOD, implement relocations, import/export
resolution, dependency order, integrity policy and a safe bootstrap path.
Built-in PC console and keyboard drivers remain necessary for early boot.

## Build

```sh
make smod-32 smod-64 smod-check
make iso-32 iso-64 img-32
```

The first test module, `hello.mod`, calls the exported
`report_ready` callback, proving that code from a native `.mod`
was executed dynamically. The loader is common code under
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
