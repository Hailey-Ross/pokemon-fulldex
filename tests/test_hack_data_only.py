from pathlib import Path
import binascii
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from test_hack_size import synthetic_crc_bytes

ROOT = Path(__file__).resolve().parents[1]


class DataOnlyHackTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_other_dumps_of_a_clean_release_stay_clean(self):
        table = (ROOT / 'frontend/romhacks.h').read_text(encoding='utf-8')
        match = re.search(r'\{ "CPUE", \d+u, 0x([0-9A-F]+)u \}', table)
        self.assertIsNotNone(match)
        target_crc = int(match.group(1), 16)
        data = bytearray(0x8004)
        data[0x0C:0x10] = b'CPUE'
        struct.pack_into('<I', data, 0x20, 0x4000)  # (the program starts with its secure area)
        struct.pack_into('<I', data, 0x2C, 0x4004)
        # A decrypted secure area, as the clean releases have.
        data[0x4000:0x4008] = bytes.fromhex('FFDEFFE7FFDEFFE7')
        data[0x8000:0x8004] = synthetic_crc_bytes(0x9E24E992)
        data += synthetic_crc_bytes(target_crc, binascii.crc32(data))
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            baseline = temp / 'synthetic-clean.bin'
            modified = temp / 'synthetic-other-dump.bin'
            baseline.write_bytes(data)
            clean = bytes(data)
            # Another dump of the same release: other bytes where the game does not look.
            dump = bytearray(clean)
            dump[0x4100] ^= 1
            modified.write_bytes(dump)
            encrypted = temp / 'synthetic-encrypted.bin'
            dump = bytearray(clean)
            dump[0x4000:0x4800] = bytes((i * 37 + 11) & 255 for i in range(0x800))
            encrypted.write_bytes(dump)
            # A hack: the program itself changed.
            hack = temp / 'synthetic-program-hack.bin'
            dump = bytearray(clean)
            dump[0x8001] ^= 1
            hack.write_bytes(dump)
            exe = temp / ('check.exe' if os.name == 'nt' else 'check')
            subprocess.run(['c++', '-std=c++17', '-pthread',
                            str(ROOT / 'tests/test_hack_data_only.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(baseline), str(modified), str(encrypted), str(hack)], check=True)
