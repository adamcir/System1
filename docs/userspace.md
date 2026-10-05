# System/1 userspace

System/1 follows UNIX/POSIX concepts where they are useful, while keeping its
own executable format and ABI.

## Filesystem layout

The current system images use:

```text
/
├── bin/        executable user programs
├── boot/       kernel and boot files
└── root/       persistent root-user files
```

The first installed test program is `/bin/test.prg`. Its source is kept in
`examples/programs/test/test.c`; image builders compile the matching SPRG and
install it into `/bin`.

## Process model

The i386 implementation executes SPRG code in CPU ring 3. The first process
model is intentionally synchronous:

```text
kernel shell
   -> execve("/bin/test.prg")
   -> child process RUNNING in ring 3
   -> _exit(status)
   -> child ZOMBIE
   -> parent reaps child
   -> kernel shell continues
```

This keeps the UNIX process lifecycle without adding `fork()`, `waitpid()` or
a scheduler before they are needed.

## i386 userspace memory

```text
0x00060000 .. 0x0006FFFF   program image
0x00070000 .. 0x0007FFFF   user stack
```

This low placement lets the same program run on the 1 MiB floppy profile.
Only those pages are user-accessible; kernel pages stay supervisor-only.

## i386 syscall ABI

Userspace enters the kernel with `int 0x80`:

```text
EAX = syscall number
EBX = argument 0
ECX = argument 1
EDX = argument 2
ESI = argument 3
EAX = return value
```

Current System/1 syscall numbers:

```text
0   read
1   write
2   open
3   close
8   lseek
9   stat
10  getcwd
11  chdir
12  mkdir
13  unlink
59  execve
60  _exit
```

These resemble common UNIX/Linux numbering where practical, but they are
System/1 ABI values rather than a Linux binary-compatibility promise.

## Test

On i386 CD or floppy:

```text
/ > ls /bin
test.prg

/ > exec /bin/test.prg
Hello from /bin/test.prg - System/1 userspace!
/ >
```

The message comes from ring 3 through the System/1 `write()` syscall and the
program returns through POSIX `_exit()`.

## x86-64

The x86-64 SPRG is built and installed in `/bin`, but x86-64 ring-3 execution
is not enabled yet. `execve()` returns `ENOSYS` there until its architecture
entry/return path is implemented.


## Shell configuration

System/1 keeps shell selection separate from the kernel shell fallback.

```text
/etc/kernel.cfg   kernel boot/userspace-shell selection
/etc/shells       valid userspace shells
/etc/msh.cfg      MultiShell behavior
```

Default `/etc/kernel.cfg`:

```text
version=1
shell=/bin/sh.prg
shells=/etc/shells
fallback=kernel
```

Default `/etc/shells`:

```text
/bin/sh.prg
/bin/msh.prg
```

`/bin/sh.prg` is a System/1 VFS symlink to `/bin/msh.prg`.

Default MultiShell configuration:

```text
version=1
path=/bin
prompt_mode=cwd
prompt_text=msh
prompt_suffix= > 
banner=1
```

With `prompt_mode=cwd`, MultiShell displays the current directory, for
example `/ > ` and `/bin > `. Set `prompt_mode=name` to use
`prompt_text` instead.

Running `kconfig` without arguments opens a small interactive kernel
configurator. `kconfig shell` opens the shell selector based on
`/etc/shells`; `kconfig shell /bin/name.prg` selects a shell directly.

The i386 syscall entry deliberately re-enables hardware interrupts after the
syscall register frame is saved. This is required for blocking TTY reads:
canonical stdin sleeps until keyboard IRQs provide a completed line.
