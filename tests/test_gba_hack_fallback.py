from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GbaHackFallbackTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('pkg-config'), 'SDL compiler tools required')
    def test_incompatible_patch_leaves_hack_untouched(self):
        flags = subprocess.run(['pkg-config', '--cflags', '--libs', 'sdl2'], capture_output=True, text=True)
        if flags.returncode:
            self.skipTest('SDL2 development package required')
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            (temp / 'mgba-util').mkdir()
            # Only hashing declarations are needed: this fixture uses a free-space safety rule.
            (temp / 'mgba-util/crc32.h').write_text(
                '#include <cstdint>\n#include <cstddef>\n'
                'inline uint32_t doCrc32(const void*, size_t) { return 0; }\n')
            (temp / 'mgba-util/sha1.h').write_text(
                '#include <cstddef>\n#include <cstdint>\n'
                'void sha1Buffer(const void*, size_t, uint8_t*);\n')
            (temp / 'patches.h').write_text(
                '#include <cstddef>\n'
                'struct WidePatch { const char* name; const unsigned char* ips; size_t ipsSize; const char* safety; };\n'
                'static const unsigned char ips[] = {80,65,84,67,72,0,0,100,0,1,153,69,79,70};\n'
                'static const WidePatch kWidePatches[] = {{"synthetic", ips, sizeof ips, "free 64 1\\n"}};\n')
            exe = temp / ('fallback.exe' if os.name == 'nt' else 'fallback')
            subprocess.run(['c++', '-std=c++17', '-I' + str(temp),
                            str(ROOT / 'tests/test_gba_hack_fallback.cpp'), '-o', str(exe),
                            *shlex.split(flags.stdout)], check=True)
            subprocess.run([str(exe), str(temp / 'synthetic.bin')], check=True)
