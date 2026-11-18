from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('void ' + name + '()')
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class GlContextFailureTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_actual_leave_and_release_with_no_context(self):
        source = (Path(os.environ.get('SSR_GL_CONTEXT_SOURCE', ROOT / 'frontend/glcontext.cpp'))).read_text()
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'context_leave.h').write_text(function(source, 'SsrLeaveGLContext') + '\n' + function(source, 'SsrReleaseGLContext'))
            exe = root / 'gl-failure'
            subprocess.run(['c++', '-std=c++17', '-I' + str(root), str(ROOT / 'tests/test_gl_context_failure.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
