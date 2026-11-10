from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CompanionTabTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'),'C++ and SDL required')
    def test_controller_binding_and_battle_tab(self):
        source=(ROOT/'frontend/main.cpp').read_text()
        start=source.index('// Controller state in SDL')
        pad=source[start:source.index('#include "touchpad.h"',start)]
        start=source.index('static PadState RemapPad(')
        remap=source[start:source.index('// The input first pressed',start)]
        with tempfile.TemporaryDirectory() as folder:
            temp=Path(folder);exe=temp/'tabs'
            (temp/'test_pad.h').write_text('#include "controls.h"\n'+pad+remap)
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            subprocess.run(['c++','-std=c++17','-I'+str(temp),'-I'+str(ROOT/'frontend'),str(ROOT/'tests/test_companion_tabs.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe)],check=True)
