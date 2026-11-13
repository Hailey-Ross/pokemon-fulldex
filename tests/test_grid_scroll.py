from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ScrollAndCompanionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_motion_and_manual_battle_tabs(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'motion'
            subprocess.run(['c++','-std=c++17','-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_grid_scroll.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
