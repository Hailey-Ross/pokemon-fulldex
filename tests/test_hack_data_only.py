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
    def test_identical_partial_fingerprints_do_not_share_clean_verdict(self):
        table = (ROOT / 'frontend/romhacks.h').read_text(encoding='utf-8')
        match = re.search(r'\{ "CPUE", \d+u, 0x([0-9A-F]+)u \}', table)
        self.assertIsNotNone(match)
        target_crc = int(match.group(1), 16)
        data = bytearray(0x8004)
        data[0x0C:0x10] = b'CPUE'
        struct.pack_into('<I', data, 0x20, 0x8000)
        struct.pack_into('<I', data, 0x2C, 4)
        data[0x8000:0x8004] = synthetic_crc_bytes(0x9E24E992)
        data += synthetic_crc_bytes(target_crc, binascii.crc32(data))
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            baseline = temp / 'synthetic-clean.bin'
            modified = temp / 'synthetic-hack.bin'
            baseline.write_bytes(data)
            data[0x4000] ^= 1  # Outside the regions used by the partial fingerprint.
            modified.write_bytes(data)
            exe = temp / ('check.exe' if os.name == 'nt' else 'check')
            subprocess.run(['c++', '-std=c++17', '-pthread',
                            str(ROOT / 'tests/test_hack_data_only.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(baseline), str(modified)], check=True)
