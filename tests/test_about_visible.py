from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AboutVisibleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_portrait_content_stays_inside_panel(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'about-visible'
            subprocess.run(['c++', '-std=c++17', '-pthread',
                            '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_about_visible.cpp'),
                            str(ROOT / 'frontend/launcher_cjk.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


# Exercise the cached composite as well as DrawPage: scroll changes must redraw it.
import os
import sys
BINARY = os.environ.get('SSR_TEST_LAUNCHER', '')

@unittest.skipUnless(sys.platform.startswith('linux') and BINARY, 'Built Linux launcher required')
class AboutLauncherScrollTests(unittest.TestCase):
    def test_scrolling_changes_the_presented_page(self):
        with tempfile.TemporaryDirectory(prefix='about-scroll-') as folder:
            root = Path(folder)
            binary = root / 'ssrecomp'
            shutil.copy2(BINARY, binary)
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith('SSR_') and k != 'LD_PRELOAD'}
            env.update(SDL_VIDEODRIVER='dummy', FULLDEX_UPDATE_URL='http://127.0.0.1:0/unavailable',
                       SSR_LIST_SIZE='720x1280', SSR_LIST_PHONE='1', SSR_LIST_PAGE='5,0')
            def capture(name, keys):
                shot = root / (name + '.bmp')
                subprocess.run([str(binary)], cwd=root,
                               env=dict(env, SSR_LIST_SHOT=str(shot), SSR_LIST_KEYS=keys),
                               timeout=20, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                return shot.read_bytes()
            self.assertTrue(capture('before', '') != capture('after', 'rdddd'), 'Scrolling did not refresh the presented page')

