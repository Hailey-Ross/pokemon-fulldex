from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherFolderSettingsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL required')
    def test_in_game_save_keeps_launcher_folders(self):
        source = (ROOT / 'frontend/main.cpp').read_text(encoding='utf-8')
        start = source.index('struct Settings : OptionalFeatures')
        end = source.index('static const char* LayoutName(', start)
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            test = temp / 'folders.cpp'
            test.write_text('''
#include <algorithm>
#include <fstream>
#include <sstream>
#include "ui.h"
#include "controls.h"
#include "achievements.h"
#include "optional_features.h"
''' + source[start:end] + '''
int main() {
    Settings settings;
    settings.Apply("roms_folder=C:/Shared ROMs\\r");
    settings.Apply("saves_folder=C:/Separate Saves\\r");
    settings.Apply("volume=50");
    settings.Apply("screen_filter=2");
    settings.Apply("gba_lcd=1");
    std::ostringstream output;
    settings.Write(output);
    const auto text = output.str();
    if (text.find("roms_folder=C:/Shared ROMs\\n") == std::string::npos) return 1;
    if (text.find("saves_folder=C:/Separate Saves\\n") == std::string::npos) return 2;
    if (text.find("volume=50\\n") == std::string::npos) return 3;
    if (text.find("screen_filter=2\\n") == std::string::npos) return 4;
    if (text.find("gba_lcd=1\\n") == std::string::npos) return 5;
}
''', encoding='utf-8')
            exe = temp / 'folders'
            flags = shlex.split(subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True))
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'),
                            '-I' + str(ROOT / 'melonDS/src'), str(test), '-o', str(exe), *flags], check=True)
            subprocess.run([str(exe)], check=True)
