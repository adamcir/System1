# System/1 Plan (Current)

## Goal
Stabilize shared module architecture, complete interactive VGA text editing, and standardize hardware control signals across all 3 outputs:
1. `iso-32`
2. `iso-64`
3. `img-32`

## Current Completed
1. Shared module base in `kernel/common/modules/*` is active.
2. `vga`, `bootlog`, `panic`, `keyboard`, `tty` wrapper implementations moved to `common`.
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
14. Temporary runtime validation trigger is wired:
   - `CTRL+ALT+BACKSPACE` -> `KEY_CTRL_ALT_BKSP` -> `signal_raise(HW_PWR_DOWN)`
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
19. `bootlog_core` now includes `vga.h` directly (no implicit VGA declarations).
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
   - `Left`, `Right`, `Insert`, `Delete`
   - `7`/`1` stay numeric (no Home/End mapping)
25. Build validation passes after keyboard/TTY/VGA update:
   - `make iso-32 iso-64 img-32`

## Active Work Items
1. Full interactive QEMU validation on all 3 targets for editor behavior parity (visual/manual pass), including:
   - no-op `Home/End` (not bound in TTY)
   - `Caps Lock` behavior (`Shift XOR CapsLock`)
   - `Tab` (4 spaces)
   - numpad numeric + navigation mode parity (`NumLock` and `Shift` inversion)
2. Verify timer/IRQ runtime parity after refactor.
3. Optional cleanup:
   - document exact VGA text API contract in module docs/comments.
4. Remove temporary `CTRL+ALT+BACKSPACE` validation trigger after final manual sign-off.

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
   - numpad navigation mode for supported keys (`Left`, `Right`, `Insert`, `Delete`)
   - scroll stability at bottom
   - `CTRL+ALT+DEL` -> `signal_raise(HW_RESET)` reset path
3. Signals checks:
   - `HW_RESET` performs immediate hardware reset
   - `CTRL+ALT+BACKSPACE` generates `KEY_CTRL_ALT_BKSP`
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
