# System/1 Module Contracts

Last updated: 2026-03-12

## Scope
This document defines the expected runtime contract for shared modules:
- `vga`
- `tty`
- `keyboard`
- `signals`
- `paging`

## `vga` Contract
Public editing API:
- `vga_text_begin(...)`
- `vga_text_putc(...)`
- `vga_text_backspace()`
- `vga_text_left()`
- `vga_text_right()`
- `vga_text_delete()`
- `vga_text_toggle_insert()`

Behavior:
- Cursor is synchronized to VGA hardware cursor.
- Scrolling up occurs when text reaches the last row.
- Insert mode toggles overwrite/insert behavior.
- `Home`/`End` helper APIs may exist in VGA internals, but TTY binding policy is separate.

Constraint:
- External modules should avoid direct VRAM writes while interactive text mode is active.

## `keyboard` Contract
Public API:
- `keyboard_init()`
- `keyboard_set_poll_fallback(uint8_t enabled)`
- `keyboard_poll()`
- `keyboard_irq_handler()`
- `keyboard_take_key()`
- `keyboard_take_char()`
- `keyboard_last_char()`

Guaranteed key events:
- Printable ASCII and control chars (`\n`, `\b`, `\t`)
- `KEY_LEFT`, `KEY_RIGHT`, `KEY_INSERT`, `KEY_DELETE`
- `KEY_CTRL_ALT_DEL`

Behavior:
- `CapsLock` affects letters only (`Shift XOR CapsLock`).
- Numpad supports numeric mode and navigation mode (`NumLock` with `Shift` inversion).
- Numpad decimal key always emits `.` regardless of `NumLock` state.
- `CTRL+ALT+BACKSPACE` is intentionally unbound (no dedicated key event).

## `tty` Contract
Public API:
- `tty_run()`

Behavior in run loop:
- Maps key events to VGA text editing operations.
- Expands `\t` to 4 spaces.
- Routes `KEY_CTRL_ALT_DEL` to `signal_raise(HW_RESET)`.
- Does not bind `Home`/`End` keys.
- Does not route `CTRL+ALT+BACKSPACE` to any signal path.

## `signals` Contract
Public API:
- `signal_raise(int signal)`
- `HW_RESET` (`0`)
- `HW_PWR_DOWN` (`1`)

Behavior:
- `HW_RESET`: i8042 reset command (`0xFE`).
- `HW_PWR_DOWN`: ACPI/PM power-off attempts (`0x604`, `0xB004`, `0x4004`), then halt fallback.

Notes:
- Current interactive keyboard path intentionally exposes only reset hotkey (`CTRL+ALT+DEL`).
- Power-down signal remains available for non-keyboard callers through `signal_raise(HW_PWR_DOWN)`.

## `paging` Contract
Public API:
- `paging_init(uint32_t boot_magic, uint32_t boot_info_ptr)`
- `paging_is_enabled()`
- `paging_identity_limit()`
- `paging_handle_page_fault()`

Behavior (V1):
- Enables paging with identity map of first 64 MiB using 4 KiB pages.
- Initialization is idempotent; repeated `paging_init` calls are no-op success.
- Boot source detection:
  - Multiboot2 path if magic is valid and mmap contains usable region.
  - Floppy/generic fallback path otherwise.
- Page fault vector (`#PF`, 14) is handled by paging module:
  - reads `CR2`
  - prints fault address
  - halts via `panic`

Limitations (V1):
- Identity mapping only; no higher-half mapping.
- No userspace split.
- No demand paging / copy-on-write.
