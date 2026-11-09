from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AboutVisibleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_portrait_content_stays_inside_panel(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'about-visible'
            subprocess.run(['c++', '-std=c++17', '-pthread',
                            '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_about_visible.cpp'),
                            str(ROOT / 'frontend/launcher_cjk.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

