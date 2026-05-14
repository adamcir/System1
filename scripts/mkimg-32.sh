#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ARTIFACT_DIR="$BUILD_DIR/artifacts"
STAGING_DIR="$BUILD_DIR/staging/floppy"
IMG="$ARTIFACT_DIR/images/system1-img-32.img"
STAGE1="$STAGING_DIR/stage1.bin"
STAGE2="$STAGING_DIR/stage2.bin"
KERNEL_ELF="$ARTIFACT_DIR/i386-floppy/kernel.elf"
KERNEL_RAW="$STAGING_DIR/kernel32.bin"

NASM_BIN="${NASM:-nasm}"
MKFS_FAT_BIN="${MKFS_FAT:-mkfs.fat}"
MCOPY_BIN="${MCOPY:-mcopy}"
DD_BIN="${DD:-dd}"
OBJCOPY_BIN="${OBJCOPY:-$(command -v i686-elf-objcopy 2>/dev/null || command -v i686-linux-gnu-objcopy 2>/dev/null || command -v objcopy 2>/dev/null)}"
PERL_BIN="${PERL:-perl}"

STAGE2_LBA=2800
MAX_STAGE2_SECTORS=79

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

mkdir -p "$STAGING_DIR" "$(dirname "$IMG")"

"$NASM_BIN" -f bin "$ROOT_DIR/boot/simple32/stage2.asm" -o "$STAGE2"

STAGE2_SIZE=$(stat -c '%s' "$STAGE2")
STAGE2_SECTORS=$(( (STAGE2_SIZE + 511) / 512 ))

if (( STAGE2_SECTORS > MAX_STAGE2_SECTORS )); then
  echo "stage2 too large: ${STAGE2_SECTORS} sectors (max ${MAX_STAGE2_SECTORS})"
  exit 1
fi

"$NASM_BIN" -f bin \
  -DSTAGE2_LBA="$STAGE2_LBA" \
  -DSTAGE2_SECTORS="$STAGE2_SECTORS" \
  "$ROOT_DIR/boot/simple32/stage1.asm" -o "$STAGE1"

"$OBJCOPY_BIN" -O binary "$KERNEL_ELF" "$KERNEL_RAW"

truncate -s 1474560 "$IMG"
"$MKFS_FAT_BIN" -F 12 -n SYSTEM1 "$IMG"

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

"$MCOPY_BIN" -i "$IMG" "$KERNEL_RAW" ::KERNEL32.BIN

echo "Created $IMG"
