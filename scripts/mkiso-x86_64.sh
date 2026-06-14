#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="$ROOT_DIR/build/staging/iso64"
ROOTFS_SRC_DIR="$ROOT_DIR/filesystem/fs64"
ROOTFS_BUILD_DIR="$ROOT_DIR/build/staging/rootfs-iso64"
ROOTFS_ISO_TMP="$ROOT_DIR/build/staging/rootfs-iso64.iso"
KERNEL_ELF="$ROOT_DIR/build/artifacts/x86_64/kernel.elf"
ROOTFS_ISO="$STAGE_DIR/boot/rootfs.iso"
OUT_ISO="$ROOT_DIR/build/artifacts/images/system1-iso-x86_64.iso"

rm -rf "$STAGE_DIR"
rm -rf "$ROOTFS_BUILD_DIR"
mkdir -p "$STAGE_DIR/boot/grub" "$(dirname "$OUT_ISO")"

if [[ ! -d "$ROOTFS_SRC_DIR" ]]; then
  echo "Missing root filesystem source: $ROOTFS_SRC_DIR"
  exit 1
fi

cp -a "$ROOTFS_SRC_DIR" "$ROOTFS_BUILD_DIR"
find "$ROOTFS_BUILD_DIR" -type f -name '.gitkeep' -delete

if [[ ! -d "$ROOTFS_BUILD_DIR/boot/grub" ]]; then
  echo "Root filesystem source must contain /boot/grub: $ROOTFS_SRC_DIR"
  exit 1
fi

if [[ ! -f "$ROOTFS_BUILD_DIR/boot/grub/grub.cfg" ]]; then
  echo "Root filesystem source must contain /boot/grub/grub.cfg: $ROOTFS_SRC_DIR"
  exit 1
fi

cp "$KERNEL_ELF" "$ROOTFS_BUILD_DIR/boot/kernel.elf"
xorriso -as mkisofs -R -J -o "$ROOTFS_ISO_TMP" "$ROOTFS_BUILD_DIR" >/dev/null 2>&1
cp "$ROOTFS_ISO_TMP" "$ROOTFS_BUILD_DIR/boot/rootfs.iso"
cp -a "$ROOTFS_BUILD_DIR"/. "$STAGE_DIR"/

grub-mkrescue -o "$OUT_ISO" "$STAGE_DIR"
