#!/usr/bin/env python3
"""System/1 native SMOD v1 packer/inspector. No ELF is stored in .mod."""
import argparse
import pathlib
import struct
import subprocess
import re

MAGIC = b"SMOD"
FMT_VERSION = 1
API_VERSION = 3
ARCH = {"i386": 1, "x86_64": 2}
HEADER = struct.Struct("<4sHHHHIIIII")
LIMIT = 16384


def parse(data: bytes, arch: str | None = None) -> dict:
    if len(data) < HEADER.size:
        raise ValueError("truncated SMOD header")
    magic, version, abi, target, flags, offset, size, memory, entry, reserved = HEADER.unpack_from(data)
    if magic != MAGIC or version != FMT_VERSION or abi != API_VERSION:
        raise ValueError("invalid magic or version")
    if arch is not None and target != ARCH[arch]:
        raise ValueError("wrong architecture")
    if target not in ARCH.values() or flags != 1 or offset != HEADER.size or reserved:
        raise ValueError("invalid flags, architecture or header")
    if not 0 < size <= LIMIT or not size <= memory <= LIMIT or entry >= size:
        raise ValueError("invalid image size or entry")
    if len(data) != offset + size:
        raise ValueError("truncated or oversized image")
    return {"arch": target, "image": size, "memory": memory, "entry": entry}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("pack", "check"))
    parser.add_argument("--arch", choices=ARCH.keys(), required=True)
    parser.add_argument("--elf", type=pathlib.Path, help="linked native code to derive BSS size")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", nargs="?", type=pathlib.Path)
    args = parser.parse_args()
    if args.action == "pack":
        if args.destination is None:
            parser.error("pack requires destination")
        image = args.source.read_bytes()
        if not 0 < len(image) <= LIMIT:
            parser.error("image must be 1..4096 bytes")
        memory_size = len(image)
        if args.elf is not None:
            symbols = subprocess.check_output(["nm", "-n", str(args.elf)], text=True)
            end = re.search(r"^([0-9a-fA-F]+) [A-Za-z] __smod_mem_end$", symbols, re.M)
            file_end = re.search(r"^([0-9a-fA-F]+) [A-Za-z] __smod_file_end$", symbols, re.M)
            if not end or not file_end:
                parser.error("linker SMOD memory symbols missing")
            memory_size = int(end.group(1), 16)
            declared_file_size = int(file_end.group(1), 16)
            if declared_file_size != len(image):
                parser.error("ELF file region and raw image sizes differ")
        if not len(image) <= memory_size <= LIMIT:
            parser.error("invalid module memory size")
        out = HEADER.pack(MAGIC, FMT_VERSION, API_VERSION, ARCH[args.arch],
                          1, HEADER.size, len(image), memory_size, 0, 0) + image
        parse(out, args.arch)
        args.destination.parent.mkdir(parents=True, exist_ok=True)
        args.destination.write_bytes(out)
    else:
        info = parse(args.source.read_bytes(), args.arch)
        print(f"SMOD v1 arch={args.arch}, code={info['image']} bytes")


if __name__ == "__main__":
    main()
