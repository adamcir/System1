import importlib.util
import pathlib
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("mksmod", ROOT / "tools" / "mksmod.py")
mksmod = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mksmod)


class NativeModuleTests(unittest.TestCase):
    def setUp(self):
        self.good = mksmod.HEADER.pack(b"SMOD", 1, 3, 1, 1, 32, 2, 2, 0, 0) + b"\x90\xc3"

    def test_valid(self):
        self.assertEqual(mksmod.parse(self.good, "i386")["image"], 2)

    def test_arch_mismatch(self):
        with self.assertRaises(ValueError):
            mksmod.parse(self.good, "x86_64")

    def test_truncated_file(self):
        with self.assertRaises(ValueError):
            mksmod.parse(self.good[:-1], "i386")

    def test_bad_magic(self):
        with self.assertRaises(ValueError):
            mksmod.parse(b"ELF!" + self.good[4:], "i386")

    def test_invalid_entry(self):
        corrupt = bytearray(self.good)
        struct.pack_into("<I", corrupt, 24, 2)
        with self.assertRaises(ValueError):
            mksmod.parse(bytes(corrupt), "i386")


if __name__ == "__main__":
    unittest.main()
