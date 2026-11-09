"""Exercise the actual screenshot overlay compositor with different output shapes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ScreenshotOverlayTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_only_matching_dimensions_are_blended(self):
        source = (ROOT / 'frontend/main.cpp').read_text()
        start = source.index('static void BlendCanvas(')
        end = source.index('static bool WriteSnapshot(', start)
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            cpp = temp / 'overlay.cpp'
            cpp.write_text(r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <vector>
struct Canvas { int w, h; std::vector<uint32_t> px; };
''' + source[start:end] + r'''
int main() {
    Canvas c{2, 2, {0xFFFFFFFF, 0x80FFFFFF, 0, 0xFF112233}};
    std::vector<uint32_t> image(4, 0xFF000000);
    BlendCanvas(c, image, 2, 2);
    assert(image[0] == 0xFFFFFFFF && image[1] == 0xFF808080);
    assert(image[2] == 0xFF000000 && image[3] == 0xFF112233);
    image.assign(4, 0xFF000000);
    // Equal area is insufficient: the row stride must match too.
    BlendCanvas(c, image, 4, 1);
    assert(image == std::vector<uint32_t>(4, 0xFF000000));
    image.assign(6, 0xFF000000);
    BlendCanvas(c, image, 3, 2);
    assert(image == std::vector<uint32_t>(6, 0xFF000000));
    image.assign(3, 0xFF000000);
    BlendCanvas(c, image, 2, 2);
    assert(image == std::vector<uint32_t>(3, 0xFF000000));
}
''')
            exe = temp / 'overlay'
            subprocess.run(['c++', '-std=c++17', str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
