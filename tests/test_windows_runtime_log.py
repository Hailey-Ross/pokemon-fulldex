from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WindowsRuntimeLogTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt' and shutil.which('c++'), 'Windows C++ compiler required')
    def test_runtime_output_and_restart(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            exe = root / 'runtime-log.exe'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-mwindows', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_windows_runtime_log.cpp'), '-o', str(exe)], check=True)
            # (Two minutes: it copies itself and starts the copies twice; a fresh program is scanned
            # before it runs, which took GitHub's Windows machines past 30 seconds. A hang still fails.)
            subprocess.run([str(exe), str(root / 'fixture')], check=True, timeout=120)
