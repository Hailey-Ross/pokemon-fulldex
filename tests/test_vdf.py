from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VdfTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_binary_vdf_round_trip(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'test-vdf'
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_vdf.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=20)


if __name__ == '__main__':
    unittest.main()
