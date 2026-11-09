"""Compile the real pre-save launch gate against a synthetic regional ROM header."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RegionalHackTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('cc'), 'C/C++ compilers required')
    def test_modified_regional_rom_requires_explicit_hack_path(self):
        main = (ROOT / 'frontend/main.cpp').read_text()
        end = main.index('    // CLOUD SAVES (cloud_autosync.h)')
        start = main.rfind('    if (const char* problem = RegionalProfiles::Check', 0, end)
        self.assertGreaterEqual(start, 0)
        gate = main[start:end]
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            source = temp / 'regional-hack.cpp'
            source.write_text(r'''
#include "regional_profiles.h"
#include <cassert>
#include <cstdio>
#include <vector>
int launchGate(const std::vector<uint8_t>& rom, bool hack, bool& recompiled) {
    struct { bool romHack, recomp; } opt{hack, true};
''' + gate + r'''
    recompiled = opt.recomp;
    return 0;
}
int main() {
    std::vector<uint8_t> rom(512);
    std::memcpy(rom.data() + 12, "IPKJ", 4);
    assert(RegionalProfiles::Check(rom.data(), rom.size()) != nullptr);
    bool recompiled = true;
    assert(launchGate(rom, false, recompiled) == 1);
    assert(launchGate(rom, true, recompiled) == 0);
    assert(!recompiled);
}
''')
            sha = temp / 'sha.o'
            subprocess.run(['cc', '-c', str(ROOT / 'melonDS/src/sha1/sha1.c'), '-o', str(sha)], check=True)
            exe = temp / 'gate'
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'),
                            str(source), str(sha), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
