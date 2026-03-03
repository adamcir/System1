# System/1 Plan (Current)

## Goal
Stabilize shared module architecture and finalize interactive text editing behavior on all 3 outputs:
1. `iso-32`
2. `iso-64`
3. `img-32`

## Current Completed
1. Common module layer introduced in `kernel/common/modules/*`.
2. Branch wrappers (`i386`, `x86_64`, `i386-floppy`) now call shared core modules where architecture/media allows.
3. Shared cores implemented:
   - `vga_core`
   - `bootlog_core`
   - `panic_core`
   - `keyboard_core`
   - `interrupts_common`
4. `Makefile` updated to build each module as `wrapper.o + core.o` into existing `lib*.a` outputs.
5. VGA improvements completed:
   - scroll-up on overflow (no wrap-to-row-0)
   - unified `vga_hex_u32` API in all branches
   - text editing API (`vga_text_begin`, `vga_text_putc`, `vga_text_backspace`, `vga_text_left`, `vga_text_right`)
   - cursor path + text buffer model for editor-like behavior
   - `Enter` inside text now performs split-line behavior (text after cursor moves to next line)
   - right arrow no longer moves beyond typed text
6. Interrupt architecture remains separated where required (IDT descriptor differences and flags save/restore path), with shared PIC/PIT/IRQ common logic.
7. Full build passes after refactor and VGA behavior updates:
   - `make iso-32 iso-64 img-32`

## Active Work Items
1. Runtime validation in QEMU for all 3 targets with focus on text editing semantics:
   - typing
   - backspace
   - left/right cursor movement bounds
   - `Enter` split in middle of line
   - newline + scroll stability
2. Validate timer/IRQ stability parity after refactor:
   - keyboard IRQ path active
   - `timer_ticks_get()` increments
3. Optional cleanup pass:
   - reduce leftover legacy VGA helper usage in kernel loops to only `vga_text_*` where appropriate
   - update docs/comments for new text editing contract

## Validation Checklist
1. `make clean && make iso-32 iso-64 img-32`.
2. Boot each target and verify:
   - no reboot loop/triple fault
   - cursor visible and synced
   - typing works
   - backspace behavior correct
   - left/right bounded by text buffer
   - enter-in-middle performs split-line
   - scrolling works when bottom is reached
3. Confirm interrupts:
   - keyboard IRQ receives input
   - timer ticks increase over time

## Risks
1. VGA text buffer/path invariants can desync if future code writes directly to cursor/screen outside `vga_text_*` contract.
2. Large text edits are bounded by `80*25` storage; behavior at hard limit must stay deterministic.
3. Any future direct screen writes from other modules can invalidate edit buffer expectations.
4. Arch-specific interrupt wrapper regressions can appear if common/arch boundaries shift without parity checks.

## Defaults / Assumptions
1. C-first architecture remains; ASM limited to entry/ISR/lidt/iret paths.
2. Existing artifact names and output paths stay unchanged.
3. Branch wrappers remain explicit for clarity, shared logic lives in `kernel/common/modules`.
4. Linker RWX warning remains accepted for current MVP.
