# Shell History And Autocomplete Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Přidat do shellu historii příkazů přes `Up/Down` a jednoznačný `Tab` autocomplete bez rozbití současného editoru řádku.

**Architecture:** `keyboard` vrstva začne vracet `KEY_UP` a `KEY_DOWN`. `tty` dostane rozšiřitelné readline hooky pro `Tab` a historii, ale zachová i stávající `tty_readline()` wrapper bez callbacků. `shell` bude držet historii a autocomplete logiku nad built-in příkazy a filesystem jmény.

**Tech Stack:** freestanding C, shared module pattern (`kernel/common/modules/*` + `kernel/*/modules/*`), statické buffery bez heapu, Makefile module builds.

---

### Task 1: Keyboard + TTY API rozšíření

**Files:**
- Modify: `kernel/common/modules/keyboard/keyboard_core.c`
- Modify: `kernel/*/modules/keyboard/keyboard.h`
- Modify: `kernel/common/modules/tty/tty.c`
- Modify: `kernel/common/modules/tty/tty-core.c`
- Modify: `kernel/common/modules/tty/tty-core.h`
- Modify: `kernel/*/modules/tty/tty.h`

**Steps:**
1. Přidat `KEY_UP` a `KEY_DOWN`.
2. Mapovat šipky `Up/Down` z extended scancode cesty i z numpad navigation mode.
3. Přidat `tty_readline_ex(..., hooks)` s callbacky pro:
   - `Tab`
   - history `Up/Down`
4. Ponechat `tty_readline()` jako kompatibilní wrapper bez hooků.

### Task 2: Shell history

**Files:**
- Modify: `kernel/common/modules/shell/shell_core.c`

**Steps:**
1. Přidat statický ring buffer historie.
2. `Up` vrací starší příkazy, `Down` novější.
3. Při prvním `Up` uložit aktuálně rozepsaný draft a při návratu dolů ho obnovit.
4. Uloženou historii nemodifikovat při editaci zobrazené kopie.

### Task 3: Jednoznačný Tab autocomplete

**Files:**
- Modify: `kernel/common/modules/shell/shell_core.c`

**Steps:**
1. Pro první token doplňovat jednoznačný built-in command match.
2. Pro další tokeny doplňovat jednoznačný filesystem match.
3. Při 0 nebo více shodách nedělat nic.
4. Po doplnění překreslit celý input řádek a ponechat kurzor na konci doplněného tokenu.

### Task 4: Verifikace

**Files:**
- None

**Steps:**
1. `make modules-32 modules-64 modules-img-32`
2. Zkontrolovat, že se v kódu objevují:
   - `KEY_UP`
   - `KEY_DOWN`
   - `tty_readline_ex`
3. Interaktivně ověřit v QEMU:
   - `Tab` na jednoznačný command prefix
   - `Tab` na jednoznačný path prefix
   - `Up/Down` přes historii
