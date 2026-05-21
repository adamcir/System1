# Write-Back RAMFS Design

## Goal

System/1 should boot from the existing CD or floppy media, copy the mounted root directory tree into a writable RAMFS working tree, and defer persistence until shutdown.

## Behavior

- CD boot: ISO9660 is the boot source. The shell works against RAMFS, so navigation and RAM-only mutations are allowed. Shutdown cannot write back to CD media and must report `read-only filesystem` if there are dirty RAMFS changes.
- Floppy boot: FAT12 is the boot source. The shell works against RAMFS. Shutdown asks whether to write dirty RAMFS changes back to the floppy image.
- Locked floppy: the write-back attempt fails and is surfaced as `read-only filesystem` or a write failure. Shutdown can still continue after the failed save.

## Architecture

The existing `fs_*` public API remains the shell boundary. `fs_core_init()` still discovers the boot media through the current block-device path, mounts FAT12 or ISO9660 read-only, then imports the visible directory tree into `ramfs_core`.

After import, `g_root_driver` switches to RAMFS. Mutating shell commands therefore update RAM only. `fs_core_shutdown()` handles the persistence boundary: it checks whether RAMFS is dirty and either refuses CD write-back or asks the floppy backend to serialize RAMFS changes into the boot floppy image and write sectors to the physical floppy.

The first implementation keeps the scope intentionally small: preserve directory listing shape, `cd`, `pwd`, and `mkdir`; import directories and file names but not file contents, because the current public FS layer does not expose file reads or writes yet.

## Components

- `ramfs_core`: adds import helpers, dirty tracking, and a VFS driver export.
- `fs_core`: mounts the boot media, imports it into RAMFS, records media type, and exposes shutdown write-back.
- `shell_core`: changes `shutdown` to call the FS shutdown hook before raising `HW_PWR_DOWN`.
- floppy write-back backend: later task writes changed RAMFS metadata back to FAT12 and then to the physical floppy. This is isolated from shell behavior.

## Error Handling

- CD/ISO write-back returns `FS_ERR_READ_ONLY`.
- No dirty changes returns `FS_OK` without prompting.
- User declines save returns `FS_OK`.
- Floppy write failure returns an FS error and leaves RAM state unchanged.

## Verification

- Build: `make modules-32 modules-64 modules-img-32`.
- Full images when toolchain is available: `make iso-32 iso-64 img-32`.
- Manual QEMU smoke:
  - CD: boot, `mkdir test`, `ls`, `shutdown`, confirm read-only write-back message.
  - FD: boot, `mkdir test`, `shutdown`, answer yes, reboot and confirm `test/` persists.
  - FD locked: run QEMU with a read-only floppy image and confirm save fails cleanly.
