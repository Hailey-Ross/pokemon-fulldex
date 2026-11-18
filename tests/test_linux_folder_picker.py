from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('c++'), 'Linux and C++ required')
class LinuxFolderPickerTests(unittest.TestCase):
    def test_desktop_preference_cancel_missing_and_shell_characters(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            cpp = root / 'picker.cpp'
            exe = root / 'picker'
            cpp.write_text('''#include "linux_folder_picker.h"
#include <iostream>
int main(int argc, char** argv) {
    auto result = LinuxFolderPicker::Choose(argv[1], argv[2]);
    std::cout << result.available << "\\n" << result.path;
}
''')
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'frontend'), str(cpp), '-o', str(exe)], check=True)
            tools = root / 'bin'
            tools.mkdir()
            log = root / 'calls'
            selected = str(root / "Pokémon ' saves $HOME `echo bad`")
            start = str(root / "ROMs ' $HOME; not-a-command")
            title = "Choose ' $(touch " + str(root / 'injected') + ")"
            env = dict(os.environ, PATH=str(tools), PICKER_LOG=str(log), PICKER_RESULT=selected)
            env.pop('KDE_FULL_SESSION', None)

            def install(name):
                path = tools / name
                path.write_text('''#!/bin/sh
printf '%s\\n' "$0" "$@" >> "$PICKER_LOG"
printf '%s\\n' "$PICKER_RESULT"
exit "${PICKER_EXIT:-0}"
''')
                path.chmod(0o755)

            def run(desktop, status='0'):
                log.write_text('')
                out = subprocess.check_output([str(exe), start, title], env=dict(env, XDG_CURRENT_DESKTOP=desktop, PICKER_EXIT=status), text=True)
                return out, log.read_text().splitlines()

            install('zenity')
            install('kdialog')
            out, calls = run('KDE:Plasma')
            self.assertEqual(out, '1\n' + selected)
            self.assertEqual(calls, [str(tools / 'kdialog'), '--getexistingdirectory', start, '--title', title])
            self.assertFalse((root / 'injected').exists())
            out, calls = run('GNOME')
            self.assertEqual(out, '1\n' + selected)
            self.assertEqual(calls, [str(tools / 'zenity'), '--file-selection', '--directory', '--filename=' + start + '/', '--title=' + title])
            out, calls = run('KDE', '1')
            self.assertEqual(out, '1\n')
            self.assertEqual(calls.count(str(tools / 'kdialog')), 1)
            self.assertNotIn(str(tools / 'zenity'), calls)
            (tools / 'kdialog').unlink()
            out, calls = run('KDE')
            self.assertEqual(out, '1\n' + selected)
            self.assertEqual(calls[0], str(tools / 'zenity'))
            (tools / 'zenity').unlink()
            out, calls = run('KDE')
            self.assertEqual(out, '0\n')
            self.assertEqual(calls, [])
