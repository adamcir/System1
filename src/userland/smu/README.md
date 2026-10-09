# System Multi Utils (SMU)

System Multi Utils is the standard userspace utility set for System/1.

SMU is intentionally **not** a multicall binary. Every command remains its own
SPRG executable in `/bin`, while the source tree is grouped as one coherent
system package.

```text
smu/
├── common/     shared SMU headers/helpers
├── shell/      MultiShell (MSh)
├── fs/         filesystem/path utilities
├── core/       small general-purpose utilities
├── editor/     AdaText
└── system/     System/1 administration utilities
```

Examples of installed programs:

```text
/bin/msh.prg
/bin/ls.prg
/bin/cat.prg
/bin/echo.prg
/bin/adatext.prg
/bin/kconfig.prg
/bin/reboot.prg
/bin/shutdown.prg
```

This layout keeps programs independently executable and debuggable, while
giving the base System/1 userland a single identity: **System Multi Utils**.


## AdaText

AdaText is the standard lightweight text editor shipped with SMU. It is a
separate SPRG executable at `/bin/adatext.prg`, not part of MSh itself.

The first version is line-oriented so it works reliably with the current
System/1 TTY ABI and low-memory floppy profile.


## Utility groups

The standard set now also includes:

```text
clear       clear the active TTY
write       create/overwrite/append text to a file
stat        show basic file metadata
sleep       delay execution by whole seconds
sync        synchronize filesystem state
true/false  return success/failure for shell conditionals
```

The Kernel Shell exposes a compact recovery subset of filesystem and
diagnostic operations, while normal interactive work should use MSh + SMU.

## Shell paths and recursive removal

The `ls` command supports `-a`, `-l`, `-d`, `-1` and
combined options like `ls -la`. `rm -r` traverses directories,
`rm -f` suppresses missing-path errors, and `rm -rf dir/` removes
the directory tree on writable FAT12 or RAMFS. The implementation
refuses deletion of the filesystem root and dot/dot-dot paths.
`cd` is a MultiShell builtin. TAB completion appends `/` when
a unique directory match is found.

`write /dev/tty Hello` sends bytes through the TTY device via VFS;
`write /dev/ttyS0 Hello` targets the COM1 UART module;
`write /dev/kmsg Hello` adds a message to the kernel log.

The x86_64 boot ISO now installs the same newly built i386 SPRG
executables as the i386 ISO; the kernel runs them using its IA-32
compatibility-mode userspace implementation.
