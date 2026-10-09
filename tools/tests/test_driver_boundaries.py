"""Architecture boundary checks for System/1 portable kernel code.

These checks are source-only and do not build or link test binaries.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "src/kernel/core"
KERNEL = ROOT / "src/kernel"


class DriverBoundaries(unittest.TestCase):
    def test_portable_core_has_no_inline_machine_code(self):
        bad = []
        for path in CORE.rglob("*.c"):
            code = path.read_text(encoding="utf-8")
            if re.search(r"\b(?:__asm__|__asm|asm)\s*(?:volatile|\()", code):
                bad.append(str(path.relative_to(ROOT)))
        self.assertEqual(bad, [], "Privileged instructions belong to arch/ or drivers/")

    def test_pc_fdc_separated_from_filesystem(self):
        core = (CORE / "fs/fs_core.c").read_text(encoding="utf-8")
        driver = (KERNEL / "drivers/pc/floppy_fdc.c").read_text(encoding="utf-8")
        self.assertNotIn("FDC_DOR", core)
        self.assertNotIn("fs_fdc_", core)
        self.assertIn("floppy_controller_read_sector", core)
        self.assertIn("pc_fdc_transfer_sector", driver)
        self.assertIn("floppy_controller_register", driver)

    def test_cpu_mmu_is_in_architecture_layer(self):
        self.assertFalse((CORE / "paging/paging_core.c").exists())
        code = (KERNEL / "arch/x86/paging_x86.c").read_text(encoding="utf-8")
        self.assertIn("%%cr3", code)
        self.assertIn("invlpg", code)

    def test_boot_platform_registered_on_each_x86_target(self):
        for arch in ("i386", "i386-floppy", "x86_64"):
            with self.subTest(arch=arch):
                code = (KERNEL / "arch" / arch / "kernel.c").read_text(encoding="utf-8")
                self.assertIn("platform_early_init();", code)
                self.assertLess(code.index("platform_early_init();"), code.index("tty_init();"))
        floppy = (KERNEL / "arch/i386-floppy/kernel.c").read_text(encoding="utf-8")
        self.assertIn("floppy_controller_platform_init();", floppy)

    def test_early_smod_stage_precedes_heap_and_fs(self):
        for arch in ("i386", "i386-floppy", "x86_64"):
            with self.subTest(arch=arch):
                code = (KERNEL / "arch" / arch / "kernel.c").read_text(encoding="utf-8")
                self.assertIn("smod_boot_early_init", code)
                self.assertLess(code.index("paging_init("), code.index("smod_boot_early_init("))
                self.assertLess(code.index("smod_boot_early_init("), code.index("mm_init("))
                self.assertLess(code.index("smod_boot_early_init("), code.index("bootstrap_init("))

    def test_preloaded_module_sources(self):
        asm = (KERNEL / "arch/i386-floppy/early_smod.S").read_text(encoding="utf-8")
        self.assertIn('.incbin "build/artifacts/modules/i386/com1.mod"', asm)
        self.assertIn('.incbin "build/artifacts/modules/i386/cmos.mod"', asm)
        for name in ("picpit", "fdc"):
            self.assertIn(f'.incbin "build/artifacts/modules/i386/{name}.mod"', asm)
        for name in ("vga", "ps2"):
            self.assertNotIn(f'.incbin "build/artifacts/modules/i386/{name}.mod"', asm)
        for arch in ("i386", "x86_64"):
            cfg = (ROOT / "rootfs" / arch / "boot/grub/grub.cfg").read_text(encoding="utf-8")
            self.assertIn("smod:com1.mod", cfg)
            self.assertIn("smod:cmos.mod", cfg)

    def test_build_includes_cpu_and_hardware_drivers(self):
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        for target in ("I386", "X64", "FLP"):
            self.assertIn(
                f"{target}_MODULE_SRCS := $(call module_all_srcs,$(KDIR_{target})) $(PC_DRIVER_SRCS) $(X86_ARCH_SRCS)",
                makefile,
            )


if __name__ == "__main__":
    unittest.main()
