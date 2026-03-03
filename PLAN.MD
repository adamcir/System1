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
5. Keyboard polling with Shift, Backspace, Left/Right arrows.

## Next Milestone: Real Interrupt Path
1. Implement real `interrupts` module internals:
   - IDT setup
   - ISR stubs
   - PIC remap
2. Switch keyboard input from polling to IRQ1 handler.
3. Keep polling as optional fallback (debug mode).
4. Add timer IRQ0 tick source.

## Planned API Additions
1. `interrupts_init()` -> full IDT/PIC init.
2. `irq_register_handler(int irq, handler)`.
3. `keyboard_irq_handler()` internal module callback.
4. `timer_ticks_get()` for timing/debug.

## Validation Steps
1. `make iso-32 iso-64 img-32` must pass.
2. Boot each target in QEMU and verify:
   - cursor visible
   - typing works
   - backspace works
   - left/right arrows move cursor
3. After IRQ integration, verify typing still works without polling loop dependence.

## Risks
1. Bad IDT setup causes triple fault.
2. Wrong PIC EOI handling can freeze keyboard IRQ.
3. x86_64 gate descriptor mismatch can break interrupts.

## Defaults / Assumptions
1. Continue with C-first approach, minimal ASM only where required.
2. Keep branch isolation (duplicate modules per branch) for clarity.
3. Continue `.bin` naming convention for kernel outputs.
