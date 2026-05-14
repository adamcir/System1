# Read-Only Disk RootFS Design

Date: 2026-05-14

## Goal

System/1 must mount a read-only filesystem from boot media as `/` on every build target. If no supported root filesystem can be mounted, kernel boot must stop with:

```c
panic("Unable to mount FS");
```

## Scope

The root filesystem is always read-only in this phase. Shell navigation commands (`pwd`, `ls`, `cd`) operate on the mounted media. Mutating commands (`mkdir`) return a read-only error instead of modifying an in-memory tree.

Supported media and formats:

- `i386-floppy`: FAT12 from the floppy image.
- `iso-32`: ISO9660 from the boot CD/DVD image.
- `iso-64`: ISO9660 from the boot CD/DVD image.
- Future physical disks: block device path designed to accept ATA/AHCI/NVMe backends later.

## Architecture

Add a small read-only VFS boundary above filesystem drivers. The shell calls only the existing `fs_*` API. Internally, `fs_init()` discovers a root source, mounts the matching read-only filesystem driver, and stores a current working directory as a path plus driver-specific node identity.

Add a block-device interface for sector reads:

- `block_read(device, lba, count, buffer)`
- fixed 512-byte logical sectors in V1
- per-target boot media discovery

Add filesystem drivers:

- FAT12 reader for floppy/root images
- ISO9660 reader for CD/DVD images

The existing RAM rootfs is removed from normal boot behavior. It can remain as test-only support if needed, but kernel boot cannot silently fall back to RAM rootfs.

## Data Flow

1. Kernel initializes low-level hardware and interrupts.
2. Kernel calls `fs_init()`.
3. `fs_init()` probes candidate root sources for the current target.
4. Probe code detects FAT12 or ISO9660.
5. Matching driver mounts as `/`.
6. Shell starts only after the mount succeeds.
7. If all probes fail, kernel calls `panic("Unable to mount FS");`.

## Error Handling

Filesystem API gains a read-only error code. Shell maps it to a clear message such as `read-only filesystem`.

Mount failures are fatal during boot. Runtime path errors remain non-fatal shell errors.

## Testing

Use host-side unit tests for parser-level filesystem logic where possible:

- FAT12 BPB/root directory recognition
- ISO9660 primary volume descriptor recognition
- read-only mutation failure
- mount failure behavior returning a distinct error to the kernel wrapper

Use build verification for all targets:

```bash
make modules-32 modules-64 modules-img-32
make iso-32 iso-64 img-32
```

Use runtime smoke tests in QEMU when available:

- boot reaches shell when media contains supported FS
- `ls /` shows contents from the mounted media
- `mkdir test` prints read-only filesystem
- corrupt/missing FS image reaches `panic("Unable to mount FS");`
