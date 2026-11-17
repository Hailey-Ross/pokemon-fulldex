from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AutoStateTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL required')
    def test_intervals_settings_and_separate_recoverable_files(self):
        main = (ROOT / 'frontend/main.cpp').read_text()
        settings = main[main.index('struct Settings : OptionalFeatures'):main.index('static const char* LayoutName(')]
        gba = (ROOT / 'gba/frontend/main.cpp').read_text()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'ds_settings.h').write_text(settings)
            (root / 'gba_settings.h').write_text(gba[gba.index('struct Setting\n'):gba.index('// Input: the keyboard')])
            start = gba.index('            static const char* const display[]')
            (root / 'gba_page_keys.h').write_text(gba[start:gba.index('            pageSettings.clear();', start)])
            exe = root / 'auto-state'
            flags = shlex.split(subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True))
            subprocess.run(['c++', '-std=c++17', '-O1', '-pthread', '-I' + str(ROOT / 'frontend'),
                            '-I' + str(ROOT / 'melonDS/src'), '-I' + str(root),
                            str(ROOT / 'tests/test_auto_state.cpp'), '-o', str(exe), *flags], check=True)
            subprocess.run([str(exe), str(root)], check=True)
