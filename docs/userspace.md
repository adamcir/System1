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

The standard installed userspace comes from System Multi Utils (SMU).
Programs under `examples/` are optional examples and are not part of the
normal build or boot images.

## Process model

The i386 implementation executes SPRG code in CPU ring 3. The first process
model is intentionally synchronous:

```text
parent shell
   -> execve("/bin/echo.prg")
   -> child process RUNNING in ring 3
   -> _exit(status)
   -> child ZOMBIE
   -> parent reaps child
   -> shell continues
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
16  ioctl
59  execve
88  reboot
60  _exit
```

These resemble common UNIX/Linux numbering where practical, but they are
System/1 ABI values rather than a Linux binary-compatibility promise.

## Running userspace

On i386 CD or floppy, normal SMU programs run from MultiShell:

```text
/ > echo Hello from System/1
Hello from System/1

/ > adatext /root/notes.txt
AdaText 0.1.0 - System Multi Utils
```

These programs execute in ring 3, use the System/1 syscall ABI, and return
through POSIX `_exit()`.

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


## MultiShell MOTD and history

MultiShell reads its runtime settings from `/etc/msh.cfg`. The default
configuration includes:

```text
motd=/etc/motd
history_file=/root/history
history_max_bytes=3072
```

At startup MSh displays the MOTD by executing the normal userspace command
`/bin/cat.prg /etc/motd`. The System/1 ASCII logo therefore belongs to the
filesystem rather than the kernel image.

Every non-empty interactive command is appended to the configured history
file. `history.prg` reads the same `history_file` setting; `history -c`
truncates it. The default filename deliberately remains FAT12 8.3 compatible.
The history size is bounded below the current FAT12 4 KiB file-write limit.

## TTY ioctl

Userspace may control terminal presentation through `ioctl()`. The first
System/1-specific TTY request is `TIOCSCOLOR`.

For the standard terminal convention, the kernel FD layer automatically
renders writes to `STDERR_FILENO` (fd 2) in red when fd 2 is a TTY. Normal
TTY output is white, and the terminal is restored to white after every stderr
write. If stderr is redirected to a regular file, no color handling occurs.

## Kernel log timestamps

Kernel log records use a monotonic timestamp derived from the 100 Hz PIT,
formatted as elapsed seconds and centiseconds from boot. This intentionally
does not depend on an RTC and is suitable for all current System/1 targets.
The normal log form is:

```text
[0.42] INFO  bootstrap: Physical filesystem mounted as root
```

The System/1 logo is no longer compiled into klog; it lives in
`/etc/motd`.


## MultiShell interactive editor and syntax

MSh owns its interactive line editor in userspace. The kernel exposes raw TTY
key events and line redraw primitives through TTY ioctl requests while normal
`read(STDIN_FILENO, ...)` remains canonical for ordinary programs.

Interactive editing includes:

- Up/Down persistent history navigation.
- Left/Right cursor movement.
- Insert/Delete and Backspace editing.
- Tab completion for commands in `/bin`, builtins, and filesystem paths.

The parser currently supports:

```text
*       wildcard matching zero or more characters
?       wildcard matching one character
'...'   literal quoted text
"..."   quoted text
\       escape next character
#       comment when starting a token
;       unconditional command separator
&&      run next command only after success
||      run next command only after failure
```

Globbing expands the final path component against directory entries. Hidden
names are not matched by `*` or `?` unless the pattern itself begins with
a dot.

Pipes and file-descriptor redirection are intentionally deferred until the
process layer has `fork()`, pipes and `dup2()`; MSh does not fake those
semantics internally.

## Reboot and shutdown

`/bin/reboot.prg` and `/bin/shutdown.prg` are normal userspace programs.
They use the System/1 reboot syscall rather than depending on Kernel Shell
builtins.


Raw interactive MSh editing uses these System/1 TTY ioctl requests:

```text
TIOCSCOLOR      set VGA/TTY foreground color
TIOCGETKEY      block for one raw keyboard event
TIOCLINEBEGIN   mark the start position of an editable line
TIOCLINEREDRAW  redraw an editable userspace line and place its cursor
```


## Streaming SPRG loader

SPRG validation/loading no longer copies the complete program file into a
fixed kernel buffer. The loader reads the 28-byte header and program headers
by offset, then streams loadable segment data in 512-byte chunks directly
from the active filesystem into the target userspace range.

The file-size ceiling is now 64 KiB without reserving a 64 KiB kernel BSS
buffer. The architecture-specific userspace memory slot remains the final
authority for whether an image actually fits in memory.


## Additional POSIX compatibility

The userspace wrappers now follow the conventional POSIX error contract:
public functions such as `open()`, `read()`, `write()`, `stat()` and
`dup2()` return `-1` on failure and set the process-local userspace
`errno`. The low-level `system1_syscall()` interface still exposes raw
negative kernel error codes.

The common userspace library provides:

```text
errno
strerror()
perror()
fstat()
dup()
dup2()
isatty()
access()
getpid()
getppid()
nanosleep()
sleep()
usleep()
```

File descriptors now refer to shared open-file descriptions. Duplicated
descriptors therefore share the same file offset, and spawned userspace
programs inherit the parent's open descriptors. This is required for future
shell redirection and pipe support.

`STDERR_FILENO` is descriptor 2 and is independent from stdout. It can be
closed or redirected with `dup2()`. When fd 2 targets a TTY, the kernel
prints it in red and immediately restores white afterwards. When fd 2 targets
a regular file, the bytes are written normally without terminal coloring.

TTY descriptors report `S_IFCHR` through `fstat()`.

System/1 does not yet implement UNIX permission bits. For `access()`,
`F_OK`, `R_OK` and `X_OK` succeed for an existing object; `W_OK`
additionally checks whether the active filesystem is writable.

The additional syscall numbers are currently:

```text
5    fstat
21   access
32   dup
33   dup2
35   nanosleep
39   getpid
89   isatty
110  getppid
```


## System Multi Utils (SMU)

The standard System/1 userland utilities are maintained as **System Multi
Utils (SMU)**.

SMU is a source/package identity, not a BusyBox-style multicall executable.
Each command stays an independent SPRG program in `/bin`. The source tree is
organized by responsibility:

```text
src/userland/smu/
├── common/
├── shell/      MultiShell (MSh)
├── fs/
├── core/
└── system/
```

This preserves the UNIX-style one-program-per-executable model while keeping
the base shell and utilities as one maintained System/1 component.


## AdaText

AdaText is the standard lightweight text editor in System Multi Utils. It is
installed as `/bin/adatext.prg`.

The current version is deliberately line-oriented because the System/1 TTY ABI
does not yet expose a complete fullscreen terminal/cursor API. This keeps the
editor reliable on both the normal i386 target and the 1 MiB floppy profile.

Basic commands:

```text
p [N]       print all text or line N
a TEXT      append a line
i N TEXT    insert before line N
r N TEXT    replace line N
d N         delete line N
e PATH      open another file
w [PATH]    save / save as
wq [PATH]   save and quit
s           status
q           quit if saved
q!          quit without saving
h           help
```

The editor uses a 3072-byte working buffer so saving stays below the current
small-file constraints of the writable FAT12 profile.


## Terminal clear and sync

`TIOCCLEAR` clears a TTY and restores the default white foreground color.
SMU `clear.prg` uses this request instead of assuming an ANSI terminal.

System/1 also exposes `sync()`. Current storage backends are synchronous:
FAT12 commits each mutation immediately, RAMFS has no backing store, and
ISO9660 is read-only. The syscall is kept as the stable POSIX-facing API for
future writeback caches.
