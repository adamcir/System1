#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE_DIR="$ROOT_DIR/build/iso32"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/boot/grub"
cp "$ROOT_DIR/build/kernel32.elf" "$STAGE_DIR/boot/kernel32.elf"
cp "$ROOT_DIR/grub/grub-i386.cfg" "$STAGE_DIR/boot/grub/grub.cfg"

grub-mkrescue -o "$ROOT_DIR/build/system1-iso-32.iso" "$STAGE_DIR"
