#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="$ROOT_DIR/build/staging/iso64"
KERNEL_BIN="$ROOT_DIR/build/artifacts/x86_64/kernel.bin"
OUT_ISO="$ROOT_DIR/build/artifacts/images/system1-iso-x86_64.iso"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/boot/grub" "$(dirname "$OUT_ISO")"
cp "$KERNEL_BIN" "$STAGE_DIR/boot/kernel64.bin"
cp "$ROOT_DIR/boot/grub/x86_64/grub.cfg" "$STAGE_DIR/boot/grub/grub.cfg"

grub-mkrescue -o "$OUT_ISO" "$STAGE_DIR"
