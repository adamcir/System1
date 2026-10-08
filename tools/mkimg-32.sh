#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ARTIFACT_DIR="$BUILD_DIR/artifacts"
STAGING_DIR="$BUILD_DIR/staging/floppy"
ROOTFS_SRC_DIR="$ROOT_DIR/rootfs/i386-floppy"
ROOTFS_BUILD_DIR="$BUILD_DIR/staging/rootfs-img32"
IMG="$ARTIFACT_DIR/images/system1-img-32.img"
STAGE1="$STAGING_DIR/stage1.bin"
STAGE2="$STAGING_DIR/stage2.bin"
KERNEL_ELF="$ARTIFACT_DIR/i386-floppy/kernel.elf"
KERNEL_RAW="$STAGING_DIR/kernel.bin"
USERLAND_DIR="$ROOT_DIR/src/userland/build/i386"

NASM_BIN="${NASM:-nasm}"
MKFS_FAT_BIN="${MKFS_FAT:-mkfs.fat}"
MCOPY_BIN="${MCOPY:-mcopy}"
MMD_BIN="${MMD:-mmd}"
MDIR_BIN="${MDIR:-mdir}"
DD_BIN="${DD:-dd}"
OBJCOPY_BIN="${OBJCOPY:-$(command -v i686-elf-objcopy 2>/dev/null || command -v i686-linux-gnu-objcopy 2>/dev/null || command -v objcopy 2>/dev/null)}"
PERL_BIN="${PERL:-perl}"

STAGE2_LBA=1
RESERVED_SECTORS=80
MAX_STAGE2_SECTORS=$(( RESERVED_SECTORS - 1 ))

if [[ ! -f "$KERNEL_ELF" ]]; then
  echo "Missing $KERNEL_ELF"
  exit 1
fi

if [[ -z "$OBJCOPY_BIN" ]]; then
  echo "Missing objcopy"
  exit 1
fi

if ! command -v "$PERL_BIN" >/dev/null 2>&1; then
  echo "Missing perl"
  exit 1
fi

if [[ ! -d "$ROOTFS_SRC_DIR" ]]; then
  echo "Missing root filesystem source: $ROOTFS_SRC_DIR"
  exit 1
fi

mkdir -p "$STAGING_DIR" "$(dirname "$IMG")"
rm -rf "$ROOTFS_BUILD_DIR"

"$NASM_BIN" -f bin "$ROOT_DIR/src/boot/simple32/stage2.asm" -o "$STAGE2"

STAGE2_SIZE=$(stat -c '%s' "$STAGE2")
STAGE2_SECTORS=$(( (STAGE2_SIZE + 511) / 512 ))

if (( STAGE2_SECTORS > MAX_STAGE2_SECTORS )); then
  echo "stage2 too large: ${STAGE2_SECTORS} sectors (max ${MAX_STAGE2_SECTORS})"
  exit 1
fi

"$NASM_BIN" -f bin \
  -DSTAGE2_LBA="$STAGE2_LBA" \
  -DSTAGE2_SECTORS="$STAGE2_SECTORS" \
  -DRESERVED_SECTORS="$RESERVED_SECTORS" \
  "$ROOT_DIR/src/boot/simple32/stage1.asm" -o "$STAGE1"

"$OBJCOPY_BIN" -O binary "$KERNEL_ELF" "$KERNEL_RAW"

cp -a "$ROOTFS_SRC_DIR" "$ROOTFS_BUILD_DIR"
find "$ROOTFS_BUILD_DIR" -type f -name '.gitkeep' -delete
mkdir -p "$ROOTFS_BUILD_DIR/bin"
cp "$USERLAND_DIR"/*.prg "$ROOTFS_BUILD_DIR/bin/"
if [[ -d "$ROOTFS_BUILD_DIR/boot" ]]; then
  cp "$KERNEL_RAW" "$ROOTFS_BUILD_DIR/boot/KERNEL.BIN"
  cp "$STAGE1" "$ROOTFS_BUILD_DIR/boot/STAGE1.BIN"
  cp "$STAGE2" "$ROOTFS_BUILD_DIR/boot/STAGE2.BIN"
fi

truncate -s 1474560 "$IMG"
# Reserve the first 80 sectors for stage1 + stage2. FAT12 starts after this
# area, so normal filesystem allocation can never overwrite the bootloader.
"$MKFS_FAT_BIN" -F 12 -R "$RESERVED_SECTORS" -n SYSTEM1 "$IMG"

"$PERL_BIN" -e '
  use strict;
  use warnings;
  my ($src, $dst, $offset) = @ARGV;
  open my $in, q{<}, $src or die "open src: $!";
  open my $out, q{+<}, $dst or die "open dst: $!";
  binmode $in;
  binmode $out;
  seek $out, $offset, 0 or die "seek: $!";
  my $buf;
  while (read($in, $buf, 65536)) {
    print {$out} $buf or die "write: $!";
  }
' "$STAGE1" "$IMG" 0

"$PERL_BIN" -e '
  use strict;
  use warnings;
  my ($src, $dst, $offset) = @ARGV;
  open my $in, q{<}, $src or die "open src: $!";
  open my $out, q{+<}, $dst or die "open dst: $!";
  binmode $in;
  binmode $out;
  seek $out, $offset, 0 or die "seek: $!";
  my $buf;
  while (read($in, $buf, 65536)) {
    print {$out} $buf or die "write: $!";
  }
' "$STAGE2" "$IMG" $(( STAGE2_LBA * 512 ))

shopt -s nullglob dotglob
ROOTFS_ITEMS=("$ROOTFS_BUILD_DIR"/*)
if (( ${#ROOTFS_ITEMS[@]} > 0 )); then
  "$MCOPY_BIN" -i "$IMG" -s "${ROOTFS_ITEMS[@]}" ::
fi
shopt -u nullglob dotglob

# Build-time sanity checks for the persistent filesystem layout.
"$MDIR_BIN" -i "$IMG" ::/boot >/dev/null
"$MDIR_BIN" -i "$IMG" ::/root >/dev/null
"$MDIR_BIN" -i "$IMG" ::/boot/KERNEL.BIN >/dev/null
"$MDIR_BIN" -i "$IMG" ::/bin/msh.prg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/bin/sh.prg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/bin/history.prg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/bin/adatext.prg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/bin/write.prg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/bin/clear.prg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/etc/kernel.cfg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/etc/msh.cfg >/dev/null
"$MDIR_BIN" -i "$IMG" ::/etc/shells >/dev/null
"$MDIR_BIN" -i "$IMG" ::/etc/motd >/dev/null

echo "Created $IMG"
