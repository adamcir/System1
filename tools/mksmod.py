#!/usr/bin/env python3
"""System/1 native SMOD v1 packer/inspector. No ELF is stored in .mod."""
import argparse
import pathlib
import struct

MAGIC = b"SMOD"
FMT_VERSION = 1
API_VERSION = 2
ARCH = {"i386": 1, "x86_64": 2}
HEADER = struct.Struct("<4sHHHHIIIII")
LIMIT = 4096


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
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", nargs="?", type=pathlib.Path)
    args = parser.parse_args()
    if args.action == "pack":
        if args.destination is None:
            parser.error("pack requires destination")
        image = args.source.read_bytes()
        if not 0 < len(image) <= LIMIT:
            parser.error("image must be 1..4096 bytes")
        out = HEADER.pack(MAGIC, FMT_VERSION, API_VERSION, ARCH[args.arch],
                          1, HEADER.size, len(image), len(image), 0, 0) + image
        parse(out, args.arch)
        args.destination.parent.mkdir(parents=True, exist_ok=True)
        args.destination.write_bytes(out)
    else:
        info = parse(args.source.read_bytes(), args.arch)
        print(f"SMOD v1 arch={args.arch}, code={info['image']} bytes")


if __name__ == "__main__":
    main()
