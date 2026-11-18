from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RuntimeKitTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_partial_install_retry_preserves_existing_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            exe = root / 'runtime-kit'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_runtime_kit.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(root / 'kit with spaces')], check=True)
