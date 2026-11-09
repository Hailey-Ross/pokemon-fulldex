# The GBA widescreen renderer and 256-pixel maps (#131): builds tests/test_gba_wide_wrap.c against
# mGBA with gba/patches/mgba-wide.patch applied and built. GBAW_MGBA: that mGBA's source folder
# (setup.sh's ~/pokemon-gba-wide/mgba); GBAW_MGBA_BUILD: the folder with its libmgba.a (static).
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
MGBA=Path(os.environ.get('GBAW_MGBA',str(Path.home()/'pokemon-gba-wide/mgba')))
BUILD=Path(os.environ.get('GBAW_MGBA_BUILD',str(MGBA/'build')))
class GbaWideWrapTests(unittest.TestCase):
 @unittest.skipUnless(shutil.which('cc') and (BUILD/'libmgba.a').exists(),'patched mGBA built as a static library required')
 def test_window_does_not_wrap(self):
  with tempfile.TemporaryDirectory() as folder:
   exe=Path(folder)/'gba-wide-wrap'
   subprocess.run(['cc','-std=gnu11','-I'+str(MGBA/'include'),'-I'+str(MGBA/'src'),'-I'+str(BUILD/'include'),str(ROOT/'tests/test_gba_wide_wrap.c'),str(BUILD/'libmgba.a'),'-lm','-lpthread','-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
if __name__=='__main__':
 unittest.main()
