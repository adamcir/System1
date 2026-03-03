#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ARTIFACT_DIR="$BUILD_DIR/artifacts"
STAGING_DIR="$BUILD_DIR/staging/floppy"
IMG="$ARTIFACT_DIR/images/system1-img-32.img"
STAGE1="$STAGING_DIR/stage1.bin"
STAGE2="$STAGING_DIR/stage2.bin"
KERNEL_BIN="$ARTIFACT_DIR/i386-floppy/kernel.bin"

NASM_BIN="${NASM:-nasm}"
MKFS_FAT_BIN="${MKFS_FAT:-mkfs.fat}"
MCOPY_BIN="${MCOPY:-mcopy}"
DD_BIN="${DD:-dd}"

STAGE2_LBA=2800
MAX_STAGE2_SECTORS=79

if [[ ! -f "$KERNEL_BIN" ]]; then
  echo "Missing $KERNEL_BIN"
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

truncate -s 1474560 "$IMG"
"$MKFS_FAT_BIN" -F 12 -n SYSTEM1 "$IMG"

"$DD_BIN" if="$STAGE1" of="$IMG" conv=notrunc bs=512 count=1 status=none
"$DD_BIN" if="$STAGE2" of="$IMG" conv=notrunc bs=512 seek="$STAGE2_LBA" status=none

"$MCOPY_BIN" -i "$IMG" "$KERNEL_BIN" ::KERNEL32.BIN

echo "Created $IMG"
