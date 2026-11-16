from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SteamShortcutsTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux Steam paths')
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_steam_entries_and_desktop_fallback(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'test-steam-shortcuts'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_steam_shortcuts.cpp'),
                            str(ROOT / 'frontend/launcher_cjk.cpp'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=60)


if __name__ == '__main__':
    unittest.main()
