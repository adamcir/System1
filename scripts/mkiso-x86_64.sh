#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="$ROOT_DIR/build/iso64"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/boot/grub"
cp "$ROOT_DIR/build/kernel64.elf" "$STAGE_DIR/boot/kernel64.elf"
cp "$ROOT_DIR/boot/grub/x86_64/grub.cfg" "$STAGE_DIR/boot/grub/grub.cfg"

grub-mkrescue -o "$ROOT_DIR/build/system1-iso-x86_64.iso" "$STAGE_DIR"
