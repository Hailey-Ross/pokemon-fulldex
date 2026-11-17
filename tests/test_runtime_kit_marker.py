from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RuntimeKitMarkerTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_marker_skips_complete_copies_only(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            exe = root / 'runtime-kit-marker'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_runtime_kit_marker.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(root / 'kit with spaces')], check=True)
