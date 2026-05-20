# System/1

System/1 is a small educational hobbyist operating system built for i386 (32 bit) and x86-64 architectures. The project features a unified modular driver structure (VGA, Keyboard, TTY, shell, signals, filesystem), a custom 16-bit Unreal Mode bootloader, and custom filesystem stacks for FAT12 (floppy) and ISO9660 (CD-ROM). The project is mainly intended for learning how an operating system works.

QEMU is the recommended environment for testing.

## Requirements on x86_64 Hosts

Install the tools required for building the kernel modules, assembly bootloaders, ISO outputs, and floppy images:

```sh
sudo apt update
sudo apt install -y \
  build-essential nasm xorriso mtools perl \
  qemu-system-x86
```

## Build

Clean the build workspace and compile all targets (CD-ROM ISOs and Floppy images):

```sh
make clean
make all
```

Or build specific targets individually:

```sh
make iso-32       # Build i386 GRUB ISO
make iso-64       # Build x86_64 GRUB ISO
make img-32       # Build i386 floppy image
```

## Output Files

After a successful build, the main artifacts are written to `build/artifacts/`:

- `build/artifacts/images/system1-iso-32.iso` (i386 CD-ROM ISO)
- `build/artifacts/images/system1-iso-x86_64.iso` (x86_64 CD-ROM ISO)
- `build/artifacts/images/system1-img-32.img` (i386 Floppy Image)
- `build/artifacts/i386/kernel.elf` (i386 CD Kernel)
- `build/artifacts/x86_64/kernel.elf` (x86_64 CD Kernel)
- `build/artifacts/i386-floppy/kernel.elf` (i386 Floppy Kernel)

## Run in QEMU

You can run your compiled builds instantly in QEMU:

### 32-bit CD-ROM ISO

```sh
qemu-system-i386 -cdrom build/artifacts/images/system1-iso-32.iso
```

### 64-bit CD-ROM ISO

```sh
qemu-system-x86_64 -cdrom build/artifacts/images/system1-iso-x86_64.iso
```

### 32-bit Floppy Image

```sh
qemu-system-i386 -fda build/artifacts/images/system1-img-32.img
```
---

*System/1 - by adamcir (Adava) AdavaSoftware in 2026. The OS is under license GPLv3.0*