from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PartialGridTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_partial_rows_and_viewport(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'grid'
            subprocess.run(['c++','-std=c++17','-O1','-pthread',
                            '-I'+os.environ.get('SSR_GRID_HEADERS',str(ROOT/'frontend')),
                            '-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_grid_partial.cpp'),
                            str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],env=dict(os.environ,SSR_GRID_PREFS=folder),check=True)
