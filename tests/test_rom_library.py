from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RomLibraryTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_nested_library_and_excluded_folders(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / ('library.exe' if os.name == 'nt' else 'library')
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(ROOT / 'tests/test_rom_library.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(Path(folder) / 'Shared ROMs')], check=True)
