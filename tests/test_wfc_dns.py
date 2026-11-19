from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WfcDnsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_only_nintendowifi_names_are_redirected(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'wfc-dns'
            subprocess.run(['c++', '-std=c++17',
                            '-I' + str(ROOT / 'melonDS/src'),
                            str(ROOT / 'tests/test_wfc_dns.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
