from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GbaWideClipTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cc'), 'C compiler required')
    def test_actual_patch_clipping(self):
        patch = (ROOT / 'gba/patches/mgba-wide.patch').read_text(encoding='utf-8')
        start = patch.index('+static bool _wideClip(')
        # (only the clipping functions: the wide menu's helpers that follow need the whole renderer)
        ends = [patch.find(mark, start) for mark in (' static void _breakWindow(', '+static bool _wideMenuLine(')]
        end = min(e for e in ends if e > start)
        functions = '\n'.join(line[1:] for line in patch[start:end].splitlines()
                              if line.startswith('+'))
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            (temp / 'wide_clip_under_test.h').write_text(functions, encoding='utf-8')
            exe = temp / ('wide_clip.exe' if os.name == 'nt' else 'wide_clip')
            subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-I' + str(temp),
                            str(ROOT / 'tests/test_gba_wide_clip.c'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
