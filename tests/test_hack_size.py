from pathlib import Path
import binascii
import os
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def synthetic_crc_bytes(target):
    # Solve CRC's 32 linear equations. This makes four synthetic bytes, not game data.
    zero = binascii.crc32(bytes(4))
    basis = {}
    for bit in range(32):
        value = binascii.crc32((1 << bit).to_bytes(4, 'little')) ^ zero
        mask = 1 << bit
        while value:
            pivot = value.bit_length() - 1
            if pivot not in basis:
                basis[pivot] = (value, mask)
                break
            value ^= basis[pivot][0]
            mask ^= basis[pivot][1]
    value, mask = target ^ zero, 0
    while value:
        row, bits = basis[value.bit_length() - 1]
        value ^= row
        mask ^= bits
    result = mask.to_bytes(4, 'little')
    assert binascii.crc32(result) == target
    return result


class ExpandedHackTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_partial_fingerprint_and_cache_cannot_override_size(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / ('hack_size.exe' if os.name == 'nt' else 'hack_size')
            subprocess.run(['c++', '-std=c++17', '-pthread',
                            str(ROOT / 'tests/test_hack_size.cpp'), '-o', str(exe)], check=True)
            path = Path(folder) / 'synthetic.bin'
            header = bytearray(0x200)
            header[0x0C:0x10] = b'IRBO'
            struct.pack_into('<I', header, 0x20, 0x8000)
            struct.pack_into('<I', header, 0x2C, 4)
            with path.open('wb') as out:
                out.write(header)
                out.seek(0x8000)
                out.write(synthetic_crc_bytes(0x749916A6))
                out.truncate(268435456 + 30720)  # Sparse synthetic fixture.
            subprocess.run([str(exe), str(path)], check=True)
