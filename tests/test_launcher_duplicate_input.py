"""Optional real-launcher regression; set SSR_TEST_LAUNCHER to the Linux binary."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = os.environ.get('SSR_TEST_LAUNCHER', '')


@unittest.skipUnless(sys.platform.startswith('linux') and BINARY and shutil.which('cc'),
                     'Built Linux launcher and C compiler required')
class LauncherDuplicateInputTests(unittest.TestCase):
    def check_sequence(self, sequence, expected):
        with tempfile.TemporaryDirectory(prefix='launcher-input-') as folder:
            temp = Path(folder)
            binary = temp / 'ssrecomp'
            shutil.copy2(BINARY, binary)
            (temp / 'roms').mkdir()
            # A synthetic header only: no game code or assets.
            header = bytearray(0x1000)
            header[:12] = b'TEST FIXTURE'
            header[12:16] = b'CPUE'
            (temp / 'roms' / 'Synthetic.nds').write_bytes(header)
            (temp / 'settings.cfg').write_text('launcher_tab2=1\n')
            injector = temp / 'input.so'
            subprocess.run(['cc', '-shared', '-fPIC',
                            str(ROOT / 'tests/launcher_duplicate_input.c'),
                            '-lSDL2', '-ldl', '-o', str(injector)], check=True)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', LD_PRELOAD=str(injector),
                       SSR_TEST_INPUT_SEQUENCE=sequence, SSR_LIST_TAB='1',
                       SSR_LIST_SIZE='1280x720', SSR_LIST_SHOT=str(temp / 'menu.bmp'),
                       SSR_LIST_SHOT_WAIT='1', SSR_UPDATE_TEST='1')
            subprocess.run([str(binary)], cwd=temp, env=env, check=True, timeout=30,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            lines = (temp / 'saves/fulldex-library.txt').read_text().splitlines()
            entries = [shlex.split(line) for line in lines if 'Synthetic.nds' in line]
            self.assertEqual(len(entries), 1)
            self.assertEqual(int(entries[0][1]), expected)

    def test_keyboard_and_controller_pair_act_once(self):
        self.check_sequence('pair', 1)

    def test_separate_presses_still_toggle_twice(self):
        self.check_sequence('separate', 0)
