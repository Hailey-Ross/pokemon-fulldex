"""Optional real Windows launcher shutdown regression, using an isolated library."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

BINARY = os.environ.get('SSR_TEST_WINDOWS_LAUNCHER', '')


@unittest.skipUnless(sys.platform == 'win32' and BINARY, 'Built Windows launcher required')
class WindowsLauncherExitTests(unittest.TestCase):
    def test_launcher_music_worker_is_joined_on_exit(self):
        with tempfile.TemporaryDirectory(prefix='launcher-exit-') as folder:
            root = Path(folder)
            original = Path(BINARY).resolve()
            shutil.copy2(original, root / 'ssrecomp.exe')
            dll = original.parent / 'SDL2.dll'
            if dll.exists():
                shutil.copy2(dll, root / dll.name)
            shot = root / 'launcher.bmp'
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith('SSR_') and key != 'FULLDEX_UPDATE_URL'}
            env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                       FULLDEX_UPDATE_URL='http://127.0.0.1:0/unavailable',
                       SSR_LIST_SHOT=str(shot), SSR_LIST_SHOT_WAIT='2',
                       SSR_LIST_SIZE='1280x800')
            result = subprocess.run([str(root / 'ssrecomp.exe')], cwd=root, env=env,
                                    timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(result.returncode, 0,
                             (root / 'game.log').read_text(errors='replace'))
            self.assertTrue(shot.exists())
