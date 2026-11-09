from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RomActionsVisibleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_controller_focus_and_manual_scroll(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'rom-actions'
            subprocess.run(['c++', '-std=c++17', '-pthread',
                            '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_rom_actions_visible.cpp'),
                            str(ROOT / 'frontend/launcher_cjk.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
