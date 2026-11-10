from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class LayoutPreviewTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_all_layout_choices_and_clicks(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'previews'
            subprocess.run(['c++','-std=c++17','-O1','-pthread','-I'+str(ROOT/'frontend'),
                            str(ROOT/'tests/test_layout_previews.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),folder],check=True)
