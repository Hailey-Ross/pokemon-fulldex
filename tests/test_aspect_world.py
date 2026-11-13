from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class AspectWorldTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL required')
    def test_fixed_aspect_world_matches_selected_view(self):
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'aspect-world'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            include=Path(os.environ.get('SSR_ASPECT_HEADERS',ROOT/'frontend'))
            subprocess.run(['c++','-std=c++17','-O1','-pthread','-I'+str(include),
                            str(ROOT/'tests/test_aspect_world.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe)],check=True)
