from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RomCheatTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_rom_profiles_and_existing_cheats(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / ('cheats.exe' if os.name == 'nt' else 'cheats')
            sources = ['tests/test_builtin_cheats.cpp', 'melonDS/src/ARCodeFile.cpp',
                       'melonDS/src/ARDatabaseDAT.cpp', 'melonDS/src/CRC32.cpp']
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'),
                            '-I' + str(ROOT / 'melonDS/src'),
                            *[str(ROOT / p) for p in sources], '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
