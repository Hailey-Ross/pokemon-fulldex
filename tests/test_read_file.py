"""Exercise the actual file reader with deterministic stdio failure injection."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReadFileTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_seek_tell_and_short_read_fail_without_invalid_allocation(self):
        main = (ROOT / 'frontend/main.cpp').read_text()
        start = main.index('static std::vector<uint8_t> ReadFile(')
        end = main.index('static bool FlushSave(', start)
        with tempfile.TemporaryDirectory() as folder:
            temp = Path(folder)
            source = temp / 'reader.cpp'
            source.write_text(r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
static int scenario, closes;
FILE* TestOpen(const char*, const char*) { return scenario == 0 ? nullptr : reinterpret_cast<FILE*>(1); }
int TestClose(FILE*) { ++closes; return 0; }
int TestSeek(FILE*, long, int where) {
    return (scenario == 1 && where == SEEK_END) || (scenario == 3 && where == SEEK_SET) ? -1 : 0;
}
long TestTell(FILE*) { return scenario == 2 ? -1 : scenario == 5 ? 0 : 3; }
size_t TestRead(void* data, size_t, size_t count, FILE*) {
    const size_t got = scenario == 4 && count ? count - 1 : count;
    if (got) std::memcpy(data, "abc", got);
    return got;
}
#define fopen TestOpen
#define fclose TestClose
#define fseek TestSeek
#define ftell TestTell
#define fread TestRead
''' + main[start:end] + r'''
int main() {
    for (scenario = 0; scenario < 7; ++scenario) {
        closes = 0;
        const auto bytes = ReadFile("synthetic");
        assert(closes == (scenario == 0 ? 0 : 1));
        if (scenario == 6) assert(std::string(bytes.begin(), bytes.end()) == "abc");
        else assert(bytes.empty());
    }
}
''')
            exe = temp / 'reader'
            subprocess.run(['c++', '-std=c++17', str(source), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
