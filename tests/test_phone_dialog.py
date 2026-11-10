"""Check phone confirmation rendering and the matching touch targets without a ROM."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PhoneDialogTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_phone_dialog(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'phone-dialog'
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT/'frontend'),
                            str(ROOT/'tests/test_phone_dialog.cpp'),
                            str(ROOT/'frontend/launcher_cjk.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, timeout=30)
