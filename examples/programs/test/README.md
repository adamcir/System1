# System/1 Test Program

This directory is the first fixture for the future System/1 program loader.

The intended final artifact is:

```text
program.prg
```

System/1 programs are `.prg` files whose first four bytes are:

```text
SPRG
```

The linker scripts here emit a small SPRG header at file offset zero and then place the program text/data after it. They are intentionally minimal and are not wired into the top-level `Makefile` yet because the kernel does not have a userspace transition or SPRG loader.

## Build

```sh
make
make check
```

Outputs:

```text
build/program-i386.prg
build/program-x86_64.prg
```

## i386 Sketch

```sh
i686-elf-gcc -ffreestanding -fno-pic -fno-pie -nostdlib -nostdinc -I../../../include -m32 -c test.c -o test.i386.o
i686-elf-ld -m elf_i386 -T linker.i386.ld --oformat=binary -o program.prg test.i386.o
```

Fallback on hosts without `i686-elf-*` may work with system `gcc -m32` and `ld -m elf_i386`.

## x86_64 Sketch

```sh
x86_64-elf-gcc -ffreestanding -fno-pic -fno-pie -nostdlib -nostdinc -I../../../include -m64 -mno-red-zone -c test.c -o test.x86_64.o
x86_64-elf-ld -m elf_x86_64 -T linker.x86_64.ld --oformat=binary -o program.prg test.x86_64.o
```

The C file currently calls System/1 user ABI wrappers. Those wrappers are still C stubs until a real syscall trap/user-mode entry exists.
