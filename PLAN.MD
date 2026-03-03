# System/1 Plan (Current)

## Goal
Stabilize input/interrupt stack and keep all 3 outputs bootable:
1. `iso-32`
2. `iso-64`
3. `img-32`

## Current Completed
1. 3-branch kernel structure (`i386`, `x86_64`, `i386-floppy`).
2. GRUB configs moved under `boot/grub/*`.
3. `.bin` kernel naming for GRUB paths.
4. Functional VGA text output with hardware cursor sync.
5. Keyboard input path supports IRQ1 handler and optional polling fallback.
6. Full interrupts stack implemented in all branches:
   - IDT setup (vectors 0..47)
   - ISR/IRQ stubs
   - PIC remap (`0x20/0x28`) + EOI
   - PIT timer IRQ0 tick source (`timer_ticks_get`)
7. `irq_register_handler(...)` now updates PIC masks (including cascade IRQ2 for slave IRQs).
8. Triple-fault reboot loop fixed by using runtime `CS` selector for IDT gates instead of hardcoded selector.
9. `iso-32` verified in QEMU `-nographic` boot path without reboot loop.

## Planned API (All 3 Branches)
1. `interrupts_init()`
2. `interrupts_enable()`
3. `interrupts_disable()`
4. `irq_register_handler(uint8_t irq, irq_handler_t handler)`
5. `timer_ticks_get()`
6. `keyboard_irq_handler()`
7. `keyboard_set_poll_fallback(uint8_t enabled)`

## Validation Steps
1. `make iso-32 iso-64 img-32` must pass.
2. Boot each target in QEMU and verify:
   - cursor visible
   - typing works
   - backspace works
   - left/right arrows move cursor
3. Verify keyboard works via IRQ path (polling fallback can stay disabled).
4. Verify timer ticks increase after boot.
5. Verify no reboot loop/triple fault in `iso-32` (`-nographic` confirmed).
6. Verify same stability on `iso-64` and `img-32`.

## Risks
1. Wrong IDT gate selector still risks triple fault if entry/bootstrap path changes.
2. Wrong PIC mask/cascade handling can silently block slave IRQs (8..15).
3. Wrong PIC EOI handling can freeze IRQ delivery.
4. x86_64 gate descriptor mismatch can break interrupts.

## Defaults / Assumptions
1. Continue C-first approach with minimal ASM (only stubs/iret/lidt path).
2. Keep branch isolation (duplicate modules per branch) for clarity.
3. Keep `.bin` naming convention for GRUB kernels.
4. Keep polling fallback disabled by default.
