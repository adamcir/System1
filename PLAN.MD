# System/1 Plan (Current)

## Execution Log
Date: 2026-05-14
1. Added first filesystem implementation plan:
   - `docs/plans/2026-05-14-ramfs-rootfs-implementation.md`
2. Added shared `fs` module as RAM rootfs:
   - new common sources: `kernel/common/modules/fs/fs.c`, `fs_core.c`, `fs_core.h`
   - new arch headers: `kernel/*/modules/fs/fs.h`
3. RAM rootfs V1 behavior:
   - static in-memory tree, no heap, no block device
   - bootstrap directories: `/boot`, `/dev`, `/etc`, `/tmp`
   - bootstrap file placeholders: `/boot/kernel`, `/etc/init`, `/dev/tty0`
4. Shell commands added:
   - `pwd`
   - `ls [path]`
   - `cd <path>`
   - `mkdir <path>`
5. Kernel boot flow updated:
   - all `kmain_*` paths now call `fs_init()` before `shell_run()`
6. Verification status:
   - `make modules-32 modules-64 modules-img-32` PASS
   - `make iso-32 iso-64 img-32` PASS
   - 5s headless boot smoke: `iso-32` PASS, `iso-64` PASS, `img-32` PASS

Date: 2026-03-05
1. Fixed one-command build fallback in `Makefile`:
   - added fallback to system `gcc`/`ld` when `i686-*` toolchain is unavailable
   - removed need for manual override (`I386_CC=... I386_LD=...`) in normal local flow
2. Fixed `panic` compile regressions:
   - `panic_core.c` now includes `vga.h` for color constants (`RED`)
   - `panic_core()` now terminates through explicit halt path to satisfy `noreturn`
3. Fixed shell cursor visibility regression:
   - `vga_core_init()` now explicitly enables VGA hardware cursor (scanline 14..15)
4. Verification status:
   - post-cursor-fix full clean build verification is pending (last run interrupted)
5. Renamed logging module from `bootlog` to `klog`:
   - common module path renamed to `kernel/common/modules/klog`
   - arch headers renamed to `kernel/*/modules/klog/klog.h`
   - API renamed from `bootlog_*` to `klog_*` across kernel and shell
6. Removed legacy TTY run path:
   - dropped `tty_run()` and `tty_core_run()` (unused)
   - retained `tty_readline()` as the shell input path
7. Removed global shell cursor state:
   - `row`/`col` in `shell_core.c` moved to locals in `shell_core_run()`
8. Verification status update:
   - post-rename full clean build run was manually interrupted
   - fresh full regression run remains pending

Date: 2026-03-04
1. Removed temporary validation trigger:
   - `CTRL+ALT+BACKSPACE` no longer generates `KEY_CTRL_ALT_BKSP`.
   - `tty` no longer routes `KEY_CTRL_ALT_BKSP` to `signal_raise(HW_PWR_DOWN)`.
2. Kept stable hardware control path:
   - `CTRL+ALT+DEL` -> `signal_raise(HW_RESET)`.
3. Completed build regression:
   - `make clean && make iso-32 iso-64 img-32` PASS.
4. Completed headless no-op verification after trigger removal:
   - `iso-32`: PASS (`CTRL+ALT+BACKSPACE` no-op)
   - `iso-64`: PASS (`CTRL+ALT+BACKSPACE` no-op)
   - `img-32`: PASS (`CTRL+ALT+BACKSPACE` no-op)
5. Implemented floppy motor idle-off in stage2 loader:
   - motor is turned off after successful kernel load
   - motor is turned off in halt path on loader errors
6. Keyboard lock/LED behavior updated:
   - default state is `CapsLock=off`, `NumLock=off`, LEDs off after init
   - keyboard LEDs are synchronized via PS/2 command `0xED`
   - numpad mode logic fixed: with `NumLock=off`, numpad digits are blocked
7. Next execution step:
   - runtime verification that floppy motor stops spinning after boot idle.
8. Started Shell V1 implementation (all targets):
   - added shared `shell` module (`shell.c`, `shell_core.c`)
   - added arch headers for `shell` (`i386`, `x86_64`, `i386-floppy`)
   - `kmain_*` switched from `tty_run()` to `shell_run()`
   - shell commands implemented: `help`, `clear`, `echo`, `reboot`, `shutdown`, `ticks`
9. Extended shared `tty` API for shell input:
   - added `tty_readline(char* buf, uint32_t cap)`
   - line editing path supports backspace/left/right/insert/delete/tab/enter
   - `CTRL+ALT+DEL` hotkey remains active via `signal_raise(HW_RESET)`
10. Fixed floppy ASCII art visibility regression:
   - shell text model now starts at current cursor (`vga_get_cursor` + `vga_text_begin`)
   - fixed escaped backslashes in floppy banner strings
11. Build validation after shell integration:
   - `make clean && make iso-32 iso-64 img-32` PASS
   - headless boot smoke (`iso-32`, `iso-64`, `img-32`) PASS

## Goal
Stabilize shared module architecture, complete interactive VGA text editing, and standardize hardware control signals across all 3 outputs:
1. `iso-32`
2. `iso-64`
3. `img-32`

## Next Milestone (Planned): Shell V1
Date: 2026-03-04
Prompt:
- `kernel> `

Scope:
1. Replace `tty_run()` boot flow with `shell_run()` on all targets:
   - `iso-32`
   - `iso-64`
   - `img-32`
2. Keep current editor behavior (insert/delete/tab/capslock/numpad rules) as input layer.
3. Add commands:
   - `help`
   - `clear`
   - `echo`
   - `reboot` (routes to `signal_raise(HW_RESET)`)
   - `shutdown` (routes to `signal_raise(HW_PWR_DOWN)`)
   - `ticks` (reads `timer_ticks_get()`)
4. Parser format:
   - `command arg1 arg2 ...` (whitespace tokenization)
   - no quoted args in V1
5. Unknown command response:
   - `unknown command`

Interface plan:
1. New shared module:
   - `kernel/common/modules/shell/shell.c`
   - `kernel/common/modules/shell/shell_core.c`
2. New arch headers:
   - `kernel/i386/modules/shell/shell.h`
   - `kernel/x86_64/modules/shell/shell.h`
   - `kernel/i386-floppy/modules/shell/shell.h`
3. `tty` API extension (for shell line input):
   - planned `tty_readline(char* buf, uint32_t cap)`

Milestone sign-off criteria:
1. `make clean && make iso-32 iso-64 img-32` PASS.
2. On each target, prompt appears as `kernel> `.
3. `help`, `clear`, `echo`, `reboot`, `shutdown`, `ticks` function identically on all targets.
4. `ticks` values increase between calls.
5. Unknown command prints exactly `unknown command`.

## Current Completed
1. Shared module base in `kernel/common/modules/*` is active.
2. `vga`, `klog`, `panic`, `keyboard`, `tty` wrapper implementations moved to `common`.
3. Arch module folders keep headers and only required arch-specific sources.
4. `interrupts` remains split as intended:
   - shared logic in `interrupts_common`
   - arch-specific wrapper in each branch
5. `Makefile` compiles common wrappers/cores for all 3 targets.
6. VGA text editing supports:
   - insert/overwrite mode toggle (`Insert`)
   - delete at cursor (`Delete`)
   - line start/end navigation (`Home`, `End`) [legacy behavior]
   - left/right cursor movement bounded by text model
   - backspace
   - newline split behavior in middle of text
   - scroll-up at bottom
7. Keyboard extended keys mapped for:
   - `Insert`
   - `Delete`
   - `Home`
   - `End`
8. Shared `tty` module integrated for all targets (`iso-32`, `iso-64`, `img-32`) and used from all `kmain_*`.
9. Keyboard supports `CTRL+ALT+DEL` mapped to `KEY_CTRL_ALT_DEL`.
10. New shared `signals` module is active for all targets:
   - `kernel/common/modules/signals/signals.c`
   - `kernel/common/modules/signals/signals_core.c`
   - arch headers in `kernel/*/modules/signals/signals.h`
11. `signals` API:
   - `signal_raise(int signal)`
   - `HW_RESET` (`0`)
   - `HW_PWR_DOWN` (`1`)
12. `tty` now routes `KEY_CTRL_ALT_DEL` to `signal_raise(HW_RESET)` (reset logic centralized in `signals_core`).
13. `signals_core` supports:
   - reset via i8042 command `0xFE`
   - power-down via ACPI/PM ports (`0x604`, `0xB004`, `0x4004`) for QEMU/Bochs variants
14. Temporary runtime validation trigger was removed after sign-off:
   - `CTRL+ALT+BACKSPACE` no longer maps to any signal action.
15. Makefile is now flexible/dynamic:
   - module list auto-discovered from `kernel/common/modules/*`
   - wrapper source resolved per arch (`kernel/<arch>/modules/<mod>/<mod>.c`) with common fallback
   - core source auto-discovered (`<mod>_core.c`, `<mod>-core.c`, `<mod>_common.c`)
16. Module build helper targets are now:
   - `modules-32`
   - `modules-64`
   - `modules-img-32`
17. Build validation passes:
   - `make iso-32 iso-64 img-32`
18. VGA headers expose explicit color-number list via `#define VGA_COLOR_*` plus enum aliases (including `YELLOW` compatibility alias).
19. `klog_core` now includes `vga.h` directly (no implicit VGA declarations).
20. `Home`/`End` key handling was intentionally removed from active TTY flow
   (reserved for future shell-era keymap decisions).
21. Keyboard now supports `Caps Lock` (letters-only effect):
   - case logic uses `Shift XOR CapsLock`
   - symbols/numbers remain `Shift`-only
22. `Tab` is now handled in `tty` as 4 spaces insertion.
23. Numpad support expanded:
   - numeric input: `0-9`, `.`, `+`, `-`, `*`, `/`, `Enter`
   - `NumLock` + `Shift` inversion supported for navigation/number mode
24. Numpad navigation mapping is active for editor-supported keys:
   - `Left`, `Right`, `Insert`
   - `7`/`1` stay numeric (no Home/End mapping)
25. Build validation passes after keyboard/TTY/VGA update:
   - `make iso-32 iso-64 img-32`
26. Build validation passes after removing temporary `CTRL+ALT+BACKSPACE` trigger:
   - `make clean && make iso-32 iso-64 img-32`
27. Keyboard lock/LED defaults and numpad mode behavior updated:
   - init defaults: `CapsLock=off`, `NumLock=off`, LEDs off
   - fixed number/navigation mode mapping for numpad (`NumLock XOR Shift`)
   - numpad decimal (`.`) emits `.` independently of `NumLock`
   - user-verified on real HW: with `NumLock=off`, numpad numbers do not type

## Active Work Items
1. Interactive filesystem shell verification on all 3 targets:
   - `pwd` returns `/`
   - `ls /` shows bootstrap tree
   - `cd /boot` works
   - `mkdir /tmp/test` works
   - `ls /tmp` shows `test/`
2. Shell cursor visibility verification on all 3 targets:
   - prompt cursor is visible and tracks editing position
   - verify both fresh boot and after `clear`
3. Logging rename runtime sanity:
   - confirm `klog` output appears in boot/shell paths where expected
   - ensure there are no remaining `bootlog_*` symbol references
4. Shell V1 interactive verification on all 3 targets:
   - prompt `kernel> `
   - commands: `help`, `clear`, `echo`, `reboot`, `shutdown`, `ticks`, `version`, `pwd`, `ls`, `cd`, `mkdir`
   - unknown command output: `unknown command`
5. Shell behavior parity (`iso-32`, `iso-64`, `img-32`):
   - same parser behavior (`command arg1 arg2 ...`)
   - same command outputs and control flow
6. Filesystem V2 planning:
   - add file read API (`cat`)
   - decide whether next step is writable ramfs files or block device layer

## Validation Checklist
1. `make clean && make iso-32 iso-64 img-32`.
2. Runtime checks per target:
   - typing, backspace
   - left/right bounds
   - insert toggle behavior
   - delete behavior
   - home/end behavior (currently unbound in TTY)
   - enter split in middle
   - tab inserts 4 spaces
   - caps lock letters-only behavior (`Shift XOR CapsLock`)
   - numpad numeric input (`0-9`, `.`, `+`, `-`, `*`, `/`, `Enter`)
   - numpad navigation mode for supported keys (`Left`, `Right`, `Insert`)
   - scroll stability at bottom
   - `CTRL+ALT+DEL` -> `signal_raise(HW_RESET)` reset path
3. Signals checks:
   - `HW_RESET` performs immediate hardware reset
   - `CTRL+ALT+BACKSPACE` has no signal side-effect
   - `HW_PWR_DOWN` attempts ACPI/PM power-down path (QEMU/Bochs compatible ports)
4. IRQ checks:
   - keyboard IRQ path active
   - `timer_ticks_get()` increases

## Validation Results
Date: 2026-03-04
Method: headless QEMU + monitor `sendkey` smoke matrix

| Target | Editor smoke | HW_RESET (`CTRL+ALT+DEL`) | HW_PWR_DOWN (`CTRL+ALT+BACKSPACE`) |
|---|---|---|---|
| `iso-32` | PASS | PASS | PASS |
| `iso-64` | PASS | PASS | PASS |
| `img-32` | PASS | PASS | PASS |

Notes:
1. Reset validated with QEMU `-no-reboot` (VM exits on reboot request).
2. Power-down validated by VM exit after key trigger.
3. Editor checks are smoke-level (no visual framebuffer assertion in headless mode); manual visual parity remains pending.
4. Build-only regression for keyboard/TTY/VGA changes completed on 2026-03-04:
   - `make iso-32 iso-64 img-32` PASS.
5. Build regression after removing temporary `CTRL+ALT+BACKSPACE` trigger:
   - `make clean && make iso-32 iso-64 img-32` PASS (2026-03-04).
6. Headless no-op regression after trigger removal:
   - `iso-32`: PASS (2026-03-04)
   - `iso-64`: PASS (2026-03-04)
   - `img-32`: PASS (2026-03-04)
7. Headless reset via monitor `sendkey ctrl-alt-delete` was non-deterministic in this environment;
   interactive verification remains required for final sign-off.
8. Floppy loader motor-off patch build validation:
   - `make img-32` PASS (2026-03-04), no assembler warnings.
9. Keyboard lock/LED + numpad logic build validations:
   - `make iso-32 iso-64 img-32` PASS (2026-03-04)
   - `make modules-32 modules-64 modules-img-32` PASS (2026-03-04)
10. Real hardware observation:
   - numpad numbers no longer type when `NumLock` is off (confirmed).
11. Additional headless key-sequence smoke regression (2026-03-04):
   - method: QEMU monitor `sendkey` sequence (`abc`, backspace, arrows, insert/delete, tab, capslock, numpad with `NumLock` toggles)
   - `iso-32`: PASS (VM remained running)
   - `iso-64`: PASS (VM remained running)
   - `img-32`: PASS (VM remained running)
12. User confirmation (2026-03-04):
   - manual `CTRL+ALT+DEL` behavior works in interactive flow
   - floppy motor idle-off behavior on `img-32` confirmed
13. Timer/IRQ runtime parity regression via QEMU interrupt trace (2026-03-04):
   - method: headless run with `-d int`, counting vectors in trace logs
   - `iso-32`: `v=20` count 704, `v=21` count 1
   - `iso-64`: `v=20` count 709, `v=21` count 1
   - `img-32`: `v=20` count 772, `v=21` count 1
   - conclusion: timer IRQ path active on all targets; parity PASS

## Risks
1. Any direct screen writes outside `vga_text_*` may desync editor model.
2. Text buffer remains bounded by screen capacity.
3. Arch interrupt wrappers still require parity checks when changed.
4. `HW_PWR_DOWN` depends on emulator/firmware support; fallback behavior is halt if shutdown is not accepted.
5. Flexible module autodiscovery may pull incomplete modules unless each module keeps consistent wrapper/core naming.

## Defaults / Assumptions
1. C-first approach with minimal ASM stays unchanged.
2. Artifact names and output paths stay unchanged.
3. Common-first module policy remains default unless arch/media constraints require local code.
4. Linker RWX warning remains accepted for MVP.

Date: 2026-03-12
1. Implemented Paging V1 module across all targets:
   - added shared module `paging` (`paging.c`, `paging_core.c`, `paging_core.h`)
   - added arch headers for `paging` (`i386`, `x86_64`, `i386-floppy`)
2. Paging V1 behavior:
   - identity map first 64 MiB with 4 KiB pages
   - i386/i386-floppy: runtime PDE/PTE build + CR3 load + CR0.PG enable
   - x86_64: runtime 4-level table rebuild (4 KiB pages) + CR3 reload
   - init path logs for multiboot2 vs fallback flow
3. Integrated paging init into boot order:
   - `kmain_*`: `vga_init()` -> `paging_init(...)` -> `interrupts_init()` -> ...
4. Added page fault integration:
   - vector `14` now dispatches to `paging_handle_page_fault()` on all targets
   - handler reports CR2 fault address and halts via `panic`
5. Verification status:
   - `make clean && make iso-32 iso-64 img-32` PASS
6. Remaining sign-off step:
   - runtime interactive verification in QEMU for paging boot path + shell parity on all targets.

Date: 2026-03-12 (update)
1. Implementation plan was saved for Paging V1 execution:
   - `docs/plans/2026-03-12-paging-v1-implementation.md`
2. Current status:
   - code integration for `paging` is in place
   - build regression (`make clean && make iso-32 iso-64 img-32`) passes
   - headless QEMU smoke (`iso-32`, `iso-64`, `img-32`) passes
3. Remaining validation for full sign-off:
   - interactive runtime pass in windowed QEMU (prompt/cursor/command parity + paging boot logs).
