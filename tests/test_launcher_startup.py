from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherStartupTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_zero_and_tiny_canvases(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'startup-layout'
            subprocess.run(['c++', '-std=c++17', '-O1', '-pthread',
                            '-I' + os.environ.get('SSR_STARTUP_HEADERS', str(ROOT / 'frontend')),
                            str(ROOT / 'tests/test_launcher_startup.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), folder], check=True)
