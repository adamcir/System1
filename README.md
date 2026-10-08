# System/1

System/1 is a small educational operating system written mainly in C and assembly. It uses a custom kernel, its own SPRG executable format, and the System Multi Utils userspace with MultiShell (MSh) and AdaText.

The main working target is i386, including a low-memory 1 MiB floppy profile. An x86-64 kernel target is also built. FAT12 is used for writable floppy media and ISO9660 for CD images. QEMU is the recommended environment for testing.

## Requirements on x86_64 Hosts

Install the tools required for building the kernel, System Multi Utils, floppy images, and GRUB-based ISO outputs:

```sh
sudo apt update
sudo apt install -y \
  build-essential gcc-multilib nasm xorriso mtools dosfstools perl \
  grub-pc-bin grub-common qemu-system-x86
```

## Download the Sources

Clone this repository:

```sh
git clone https://github.com/adamcir/System1
cd System1
```

## Build

Build all current System/1 images:

```sh
make all
```

The build can also be run step by step:

```sh
make modules-32
make modules-64
make modules-img-32
make user-programs
make iso-32
make iso-64
make img-32
```

`make user-programs` builds the standard System Multi Utils programs. Example and test programs under `examples/` are not built automatically.

## System Multi Utils

System Multi Utils (SMU) is the standard userspace utility set for System/1. It is not a multicall binary: every utility is compiled as its own SPRG executable in `/bin`.

The standard set includes MultiShell (MSh), filesystem utilities, system administration commands, and AdaText.

Examples:

```text
/bin/msh.prg
/bin/ls.prg
/bin/cat.prg
/bin/echo.prg
/bin/adatext.prg
/bin/kconfig.prg
/bin/reboot.prg
/bin/shutdown.prg
```

AdaText can open an existing file or start a new one:

```sh
adatext /root/notes.txt
```

## Output Files

After a successful build, the main artifacts are written to `build/artifacts/`:

- `build/artifacts/images/system1-iso-32.iso`
- `build/artifacts/images/system1-iso-x86_64.iso`
- `build/artifacts/images/system1-img-32.img`
- `build/artifacts/i386/kernel.elf`
- `build/artifacts/x86_64/kernel.elf`
- `build/artifacts/i386-floppy/kernel.elf`

## Run in QEMU

### i386 ISO

```sh
make run-32
```

### x86-64 ISO

```sh
make run-64
```

The x86-64 kernel target is built and bootable, but the complete ring-3 userspace execution path is still focused on i386.

### i386 Floppy

```sh
make run-img-32
```

The floppy target keeps the 1 MiB low-memory profile and uses a writable FAT12 root filesystem.

---
*System/1 - by Adam Cir (Adava), Adava Software / Adava Development in 2026. The OS is under license GPLv3.*
