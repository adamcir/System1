# System/1

System/1 is a small educational hobby operating system written mainly in C and assembly. It currently targets i386, x86-64 and a custom i386 floppy boot path.

The kernel uses a RAMFS-first bootstrap: RAMFS is created as the initial fallback root, then FAT12 or ISO9660 boot media is detected. When physical media is available, that filesystem becomes `/` directly. A writable FAT12 floppy is modified immediately by create, write, mkdir and unlink operations; there is no shutdown-time writeback queue. If no physical filesystem is available, System/1 continues on an empty RAMFS and starts the built-in kernel shell.

## Repository layout

```text
src/
  boot/                    boot sources
  kernel/core/             shared kernel modules
  kernel/arch/             architecture-specific kernel code
  lib/user/                userspace support library
include/system1/           public System/1 headers
rootfs/                    filesystem source trees for boot images
tools/                     image builders and linker scripts
docs/                      architecture notes and development plans
examples/                  example System/1 programs
```

See `docs/architecture.md` for the detailed layout and bootstrap flow, and `docs/userspace.md` for the SPRG/process/syscall ABI.

## Requirements on x86_64 hosts

```sh
sudo apt update
sudo apt install -y \
  build-essential nasm xorriso mtools dosfstools perl \
  qemu-system-x86
```

## Build

```sh
make clean
make all
```

Individual targets:

```sh
make iso-32
make iso-64
make img-32
```

## Output files

Build artifacts are written to `build/artifacts/`:

- `build/artifacts/images/system1-iso-32.iso`
- `build/artifacts/images/system1-iso-x86_64.iso`
- `build/artifacts/images/system1-img-32.img`
- `build/artifacts/i386/kernel.elf`
- `build/artifacts/x86_64/kernel.elf`
- `build/artifacts/i386-floppy/kernel.elf`

## Run in QEMU

```sh
make run-32
make run-64
make run-img-32
```

The ISO targets use more RAM for GRUB and CD boot. The floppy target keeps the 1 MiB low-memory profile.

---

System/1 — Adam Cír (Adava / Adava Software), 2026. Licensed under GPLv3.
