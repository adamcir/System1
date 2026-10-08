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
