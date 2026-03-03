# System/1 Plan (Current)

## Goal
Stabilize shared module architecture and complete interactive VGA text editing across all 3 outputs:
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
   - line start/end navigation (`Home`, `End`)
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
10. `tty` handles `KEY_CTRL_ALT_DEL` by triggering hard reset via i8042 command `0xFE`.
11. VGA headers expose explicit color-number list via `#define VGA_COLOR_*` plus enum aliases (including `YELLOW` compatibility alias).
12. `bootlog_core` now includes `vga.h` directly (no implicit VGA declarations).
13. Build validation passes:
   - `make iso-32 iso-64 img-32`

## Active Work Items
1. Full interactive QEMU validation on all 3 targets for editor behavior parity.
2. Verify timer/IRQ runtime parity after refactor.
3. Optional cleanup:
   - document exact VGA text API contract in module docs/comments.

## Validation Checklist
1. `make clean && make iso-32 iso-64 img-32`.
2. Runtime checks per target:
   - typing, backspace
   - left/right bounds
   - insert toggle behavior
   - delete behavior
   - home/end behavior
   - enter split in middle
   - scroll stability at bottom
   - ctrl+alt+del reset path
3. IRQ checks:
   - keyboard IRQ path active
   - `timer_ticks_get()` increases

## Risks
1. Any direct screen writes outside `vga_text_*` may desync editor model.
2. Text buffer remains bounded by screen capacity.
3. Arch interrupt wrappers still require parity checks when changed.

## Defaults / Assumptions
1. C-first approach with minimal ASM stays unchanged.
2. Artifact names and output paths stay unchanged.
3. Common-first module policy remains default unless arch/media constraints require local code.
4. Linker RWX warning remains accepted for MVP.
