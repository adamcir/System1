# RAM RootFS Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Přidat první kernelový filesystem jako statický RAM rootfs s unixovou cestovou semantikou a napojit ho do shellu.

**Architecture:** Bootloader zůstane beze změny na FAT12 image a bude dál jen načítat kernel. Uvnitř kernelu vznikne nový shared modul `fs`, který po bootu vytvoří pevný strom uzlů v paměti, drží current working directory a poskytne jednoduché path API pro shell (`pwd`, `ls`, `cd`, `mkdir`).

**Tech Stack:** freestanding C, shared module pattern (`kernel/common/modules/*` + `kernel/*/modules/*`), statická alokace bez heapu, QEMU smoke testy přes existující build targety.

---

### Task 1: FS modul a veřejné API

**Files:**
- Create: `kernel/common/modules/fs/fs.c`
- Create: `kernel/common/modules/fs/fs_core.c`
- Create: `kernel/common/modules/fs/fs_core.h`
- Create: `kernel/i386/modules/fs/fs.h`
- Create: `kernel/x86_64/modules/fs/fs.h`
- Create: `kernel/i386-floppy/modules/fs/fs.h`

**Steps:**
1. Přidat veřejné API:
   - `int fs_init(void);`
   - `const char* fs_get_cwd_path(void);`
   - `int fs_change_dir(const char* path);`
   - `int fs_make_dir(const char* path);`
   - `int fs_list_dir(const char* path, fs_dirent_t* entries, uint32_t cap, uint32_t* out_count);`
2. Zavést statické limity pro V1:
   - pevný počet uzlů
   - pevný počet dětí na adresář
   - pevný name/path buffer
3. Přidat návratové kódy pro chyby (`not found`, `exists`, `not dir`, `invalid`, `no space`).

### Task 2: RAM rootfs strom a path lookup

**Files:**
- Modify: `kernel/common/modules/fs/fs_core.c`

**Steps:**
1. Implementovat root uzel `/` a bootstrap strom:
   - `/boot`
   - `/dev`
   - `/etc`
   - `/tmp`
2. Přidat placeholder file uzly pro budoucí systém:
   - `/boot/kernel`
   - `/etc/init`
   - `/dev/tty0`
3. Implementovat absolute/relative path lookup:
   - `/`
   - `.`
   - `..`
   - více segmentů oddělených `/`
4. `mkdir` v1:
   - vytváří jen poslední segment
   - parent musí existovat
   - bez rekurzivního `mkdir -p`
5. `cwd` držet jako ukazatel na uzel; textovou podobu cesty skládat on-demand.

### Task 3: Kernel init a shell příkazy

**Files:**
- Modify: `kernel/i386/kernel.c`
- Modify: `kernel/x86_64/kernel.c`
- Modify: `kernel/i386-floppy/kernel.c`
- Modify: `kernel/common/modules/shell/shell_core.c`

**Steps:**
1. V každém `kmain_*` zavolat `fs_init()` po základním kernel initu a před `shell_run()`.
2. Rozšířit shell help a parser o:
   - `pwd`
   - `ls [path]`
   - `cd <path>`
   - `mkdir <path>`
3. Přidat konzistentní textové chybové hlášky z FS návratových kódů.
4. Zachovat kompatibilitu se stávajícími příkazy (`help`, `clear`, `echo`, `reboot`, `shutdown`, `ticks`, `version`).

### Task 4: Verifikace a projektový kontext

**Files:**
- Modify: `PLAN.MD`
- Modify: `CONTEXT.TXT`

**Steps:**
1. Build verification:
   - `make modules-32 modules-64 modules-img-32`
   - `make iso-32 iso-64 img-32`
2. Runtime smoke:
   - `pwd` vrací `/`
   - `ls /` vypíše bootstrap adresáře
   - `cd /boot` funguje
   - `mkdir /tmp/test` vytvoří nový adresář
   - `ls /tmp` vypíše `test/`
3. Zapsat do `PLAN.MD` a `CONTEXT.TXT`, že boot stále běží přes FAT12 loader a kernel používá vlastní RAM rootfs.
