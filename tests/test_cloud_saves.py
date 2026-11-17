"""Cloud saves: the engine's tests (tests/test_cloud_saves.cpp) and, against tests/fake_drive.py,
the Google Drive path. Linux only: the test uses the libcurl transport that the frontend builds
there (Mac and Windows have their own in achievements_http_apple.mm / WinHTTP)."""
from pathlib import Path
import ctypes.util
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
FRONTEND = ROOT / 'frontend'
SOURCES = [str(ROOT / 'tests/test_cloud_saves.cpp'), str(FRONTEND / 'cloud_saves.cpp'),
           str(FRONTEND / 'achievements_http.cpp')]
LINUX = sys.platform.startswith('linux')


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def build(exe, extra=()):
    subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(FRONTEND), *extra, *SOURCES,
                    '-ldl', '-o', str(exe)], check=True)


def wait_for(port):
    for _ in range(100):
        with socket.socket() as s:
            if s.connect_ex(('127.0.0.1', port)) == 0:
                return True
        time.sleep(0.05)
    return False


class CloudSaveTests(unittest.TestCase):
    @unittest.skipUnless(LINUX and shutil.which('c++'), 'the test uses the Linux C++ transport')
    def test_folder_and_webdav_engine(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'cloud-saves'
            build(exe)
            subprocess.run([str(exe)], check=True, timeout=120)

    @unittest.skipUnless(LINUX and shutil.which('c++'), 'the test uses the Linux C++ transport')
    @unittest.skipUnless(ctypes.util.find_library('curl'), 'libcurl required')
    def test_google_drive_engine(self):
        port = free_port()
        server = subprocess.Popen([sys.executable, str(ROOT / 'tests/fake_drive.py'), str(port)],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            self.assertTrue(wait_for(port), 'fake Drive did not start')
            with tempfile.TemporaryDirectory() as folder:
                exe = Path(folder) / 'cloud-saves-drive'
                base = 'http://127.0.0.1:%d' % port
                build(exe, ['-DFULLDEX_TEST_DRIVE',
                            '-DFULLDEX_GOOGLE_OAUTH="%s"' % base,
                            '-DFULLDEX_GOOGLE_API="%s"' % base])
                subprocess.run([str(exe)], check=True, timeout=120)
        finally:
            server.terminate()
            server.wait(timeout=10)


if __name__ == '__main__':
    unittest.main()
