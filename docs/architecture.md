# System/1 source layout

System/1 keeps platform-independent code separate from architecture and boot-media code.

```text
src/
  boot/                    early boot sources and custom floppy loader
  kernel/
    core/                  architecture-independent kernel modules
    arch/
      i386/                i386 entry, ISR and architecture wrappers
      i386-floppy/         i386 custom-floppy target
      x86_64/              x86-64 entry, ISR and architecture wrappers
  lib/
    user/                  userspace support library

include/
  system1/                 public System/1 userspace headers

rootfs/
  i386/                    i386 CD filesystem source
  i386-floppy/             FAT12 floppy filesystem source
  x86_64/                  x86-64 CD filesystem source

tools/
  linker/                  kernel linker scripts
  mkiso-i386.sh
  mkiso-x86_64.sh
  mkimg-32.sh

docs/                      design notes and historical plans
examples/                  example System/1 programs
```

## Early bootstrap

The kernel does not depend on a userspace `init` yet.

Current boot order:

```text
bootloader
  -> architecture kernel entry
  -> paging / memory / interrupts / keyboard
  -> kernel bootstrap
  -> RAMFS mounted as /
  -> optional FAT12 or ISO9660 detection
  -> physical filesystem used as backing media when available
  -> syscalls
  -> built-in kernel shell
```

RAMFS is mandatory. Physical media is optional: failure to find or mount a
physical filesystem must not prevent the built-in shell from starting.

When executable process loading is ready, the kernel bootstrap can hand off to
an `init.prg` process instead of starting the built-in shell directly.


## Floppy memory model

The i386 floppy target does not mirror the complete 1.44 MB disk in RAM.
The stage-2 loader keeps only the FAT12 boot sector, FAT and root-directory
metadata in low memory. File and subdirectory sectors are read through the
floppy controller on demand. This keeps RAMFS independent from the physical
disk image and avoids requiring memory at 0x00200000 on the 1 MiB profile.


## Live physical filesystem mode

RAMFS is only the bootstrap and fallback root. After FAT12 or ISO9660 media is
successfully detected, the physical filesystem becomes the active `/`.

For writable FAT12 floppy media, filesystem mutations are synchronous:

```text
mkdir /root/test
  -> FAT12 directory entry + cluster written to floppy immediately

write /root/file.txt ...
  -> FAT12 data/FAT/directory entry written to floppy immediately

rm /root/file.txt
  -> FAT12 directory entry deleted and cluster chain released immediately
```

There is no dirty-file queue and shutdown/reboot does not ask whether changes
should be saved. RAMFS is empty by default for now; it does not pre-create
`/dev`, `/tmp`, `/run` or `/mnt`. The floppy image source currently
contains only the intended persistent top-level trees such as `/boot` and
`/root`.


## Floppy boot reservation

The custom FAT12 floppy reserves sectors 0..79 for the boot path:

```text
LBA 0       stage1 + FAT12 BPB
LBA 1..79   reserved stage2 area
LBA 80..    FAT12 tables, root directory and data
```

This is intentional. The live FAT12 allocator must never be able to reuse the
sectors containing stage2. Stage2 also keeps the FAT12 root directory and the
`/boot` directory in separate buffers; loading `/boot` must not overwrite
the root-directory cache exported to the kernel.
