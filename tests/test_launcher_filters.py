from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherFilterTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL required')
    def test_display_options_and_two_way_gba_persistence(self):
        source = (ROOT / 'gba/frontend/main.cpp').read_text()
        start = source.index('struct Setting\n')
        end = source.index('// Input: the keyboard', start)
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'gba_settings_under_test.h').write_text(source[start:end])
            flags = subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True).split()
            for platform, extra in [('desktop', []), ('switch', ['-D__SWITCH__'])]:
                with self.subTest(platform=platform):
                    data = root / platform
                    data.mkdir()
                    exe = root / ('filters-' + platform)
                    subprocess.run(['c++', '-std=c++17', '-O1', '-pthread', *extra,
                                    '-I' + str(ROOT / 'frontend'), '-I' + str(root),
                                    str(ROOT / 'tests/test_launcher_filters.cpp'),
                                    '-o', str(exe), *flags], check=True)
                    subprocess.run([str(exe), str(data)], check=True)
