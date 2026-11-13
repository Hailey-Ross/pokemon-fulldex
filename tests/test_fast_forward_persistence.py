from pathlib import Path
import shlex
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FastForwardPersistenceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL required')
    def test_pause_choices_survive_relaunch_and_ds_gba_switching(self):
        ds = (ROOT / 'frontend/main.cpp').read_text()
        gba = (ROOT / 'gba/frontend/main.cpp').read_text()
        settings = ds[ds.index('struct Settings : OptionalFeatures'):ds.index('static const char* LayoutName(')]
        gbaconfig = gba[gba.index('struct Setting\n'):gba.index('// Input: the keyboard')]
        dsaction = re.search(r'case M_FAST:\s*(fastLock = !fastLock;.*?)\s*break;', ds, re.S).group(1)
        gbaaction = gba.split('if (item == "FAST FORWARD" && (in.left || in.right || in.accept))', 1)[1]
        gbaaction = gbaaction.split('{', 1)[1].split('pause.items[', 1)[0]
        dsinit = next(line.strip() for line in ds.splitlines() if line.strip().startswith('bool fastLock = cfg.'))
        gbainit = next(line.strip() for line in gba.splitlines() if line.strip().startswith('fastToggle = set.Get('))
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            (temp / 'ds_settings.h').write_text(settings)
            (temp / 'gba_settings.h').write_text(gbaconfig)
            (temp / 'ff_actions.h').write_text('''
bool DsStartsFast(const DsConfig::Settings& cfg) { ''' + dsinit + ''' return fastLock; }
bool GbaStartsFast(const GbaConfig::Settings& set) { bool fastToggle; ''' + gbainit + ''' return fastToggle; }
void ToggleDs(DsConfig::Settings& cfg, const std::string& path, bool& fastLock) {
    struct { std::string config; } opt{path};
''' + dsaction + '''
}
void ToggleGba(GbaConfig::Settings& set, bool& fastToggle) {
''' + gbaaction + '\n}\n')
            flags = shlex.split(subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True))
            for platform, defines in [('desktop', []), ('switch', ['-D__SWITCH__'])]:
                with self.subTest(platform=platform):
                    data = temp / platform
                    data.mkdir()
                    exe = temp / ('persistence-' + platform)
                    subprocess.run(['c++', '-std=c++17', '-O1', '-pthread', *defines,
                                    '-I' + str(ROOT / 'frontend'), '-I' + str(ROOT / 'melonDS/src'),
                                    '-I' + str(temp), str(ROOT / 'tests/test_fast_forward_persistence.cpp'),
                                    '-o', str(exe), *flags], check=True)
                    subprocess.run([str(exe), str(data)], check=True)
