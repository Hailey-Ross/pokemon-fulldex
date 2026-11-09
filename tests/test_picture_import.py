"""Import synthetic pictures through the real persistence and artwork-loading code."""
from pathlib import Path
import binascii
import os
import shlex
import sys
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]


def png(colour):
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', binascii.crc32(kind + payload))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', 1, 1, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(b'\0' + bytes(colour))) + chunk(b'IEND', b''))


class PictureImportTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('cc'), 'C/C++ compilers required')
    def test_independent_artwork_and_browser_filtering(self):
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            red, blue = png((255, 0, 0, 255)), png((0, 0, 255, 255))
            (temp / 'Red picture.PNG').write_bytes(red)
            # Content detection, rather than a filename extension, determines the stored format.
            (temp / 'Blue.jpeg').write_bytes(blue)
            (temp / 'Invalid.png').write_bytes(b'not an image')
            sha = temp / 'sha.o'
            subprocess.run(['cc', '-c', str(ROOT / 'melonDS/src/sha1/sha1.c'), '-o', str(sha)], check=True)
            exe = temp / 'picture-import'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_picture_import.cpp'), str(sha), '-o', str(exe)], check=True)
            subprocess.run([str(exe), str(temp)], check=True)
            self.assertEqual((temp / 'Red picture.PNG').read_bytes(), red)
            self.assertEqual((temp / 'Blue.jpeg').read_bytes(), blue)

    @unittest.skipUnless(sys.platform.startswith('linux') and os.environ.get('SSR_TEST_LAUNCHER') and shutil.which('cc'),
                         'Built Linux launcher and C compiler required')
    def test_controller_can_choose_picture_in_real_browser(self):
        self.check_browser(False)

    @unittest.skipUnless(sys.platform.startswith('linux') and os.environ.get('SSR_TEST_LAUNCHER') and shutil.which('cc'),
                         'Built Linux launcher and C compiler required')
    def test_resizing_does_not_skip_or_repeat_files_between_pages(self):
        self.check_browser(True)

    def check_browser(self, paging):
        with tempfile.TemporaryDirectory(prefix='picture-browser-') as folder:
            temp = Path(folder)
            binary = temp / 'ssrecomp'
            shutil.copy2(os.environ['SSR_TEST_LAUNCHER'], binary)
            (temp / 'roms').mkdir()
            header = bytearray(0x1000)
            header[:12] = b'TEST FIXTURE'
            header[12:16] = b'CPUE'
            (temp / 'roms/Synthetic.nds').write_bytes(header)
            image = png((255, 0, 0, 255))
            if paging:
                for i in range(6):
                    (temp / f'roms/Picture{i}.PNG').write_bytes(image if i == 5 else png((0, 0, 255, 255)))
            else:
                (temp / 'roms/Red.PNG').write_bytes(image)
            (temp / 'settings.cfg').write_text('launcher_tab2=1\n')
            injector = temp / 'input.so'
            subprocess.run(['cc', '-shared', '-fPIC', str(ROOT / 'tests/picture_browser_input.c'),
                            '-lSDL2', '-ldl', '-o', str(injector)], check=True)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', LD_PRELOAD=str(injector),
                       SSR_LIST_DIALOG='20', SSR_LIST_TAB='1', SSR_LIST_SIZE='1280x720',
                       SSR_LIST_SHOT=str(temp / 'menu.bmp'), SSR_LIST_SHOT_WAIT='2', FULLDEX_UPDATE_URL='http://127.0.0.1:0/unavailable')
            if paging:
                env['SSR_PICTURE_PAGE_TEST'] = '1'
                env['SSR_LIST_SHOT_WAIT'] = '4'
            subprocess.run([str(binary)], cwd=temp, env=env, check=True, timeout=30,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            choice_file = temp / 'saves/fulldex-art.txt'
            self.assertTrue(choice_file.exists(), (temp / 'game.log').read_text())
            choices = choice_file.read_text().splitlines()
            entries = [shlex.split(line) for line in choices if 'Synthetic.nds' in line]
            self.assertEqual(len(entries), 1)
            self.assertEqual((temp / 'saves/artwork' / entries[0][1]).read_bytes(), image)
            original = temp / ('roms/Picture5.PNG' if paging else 'roms/Red.PNG')
            self.assertEqual(original.read_bytes(), image)
