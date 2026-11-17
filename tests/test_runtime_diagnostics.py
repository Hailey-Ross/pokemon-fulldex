from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RuntimeDiagnosticsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_changed_runtime_logs_are_bounded_and_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            exe = root / ('diagnostics.exe' if os.name == 'nt' else 'diagnostics')
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_runtime_diagnostics.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(root / 'fixture')], check=True, timeout=30)
