from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DisplayPacingTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_present_deadlines_with_ignored_and_working_vsync(self):
        source = (ROOT / 'frontend/main.cpp').read_text()
        present = source[source.index('    auto presentFrame ='):source.index('    auto drawFrame =')]
        harness = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
static uint64_t clockNow, lastDriverReturn;
static bool driverBlocks;
static uint64_t SDL_GetPerformanceCounter() { return ++clockNow; }
static uint32_t SDL_GetTicks() { return clockNow / 1000; }
static void SleepPrecise(double seconds) { clockNow += (uint64_t)(seconds * 1000000); }
static void SDL_RenderPresent(void*) {
    if (driverBlocks) {
        uint64_t next = lastDriverReturn + 16667;
        while (next < clockNow) next += 16667;
        clockNow = next;
        lastDriverReturn = next;
    }
}
struct DrawJob { bool wantShot = false; };
static void Run(bool blocks, bool sync, bool swapAccepted) {
    clockNow = 1; lastDriverReturn = 0; driverBlocks = blocks;
    bool syncToDisplay = sync, swapPaced = swapAccepted, pipelined = false, perfOn = false;
    int swapQuick = 0;
    uint64_t perfFreq = 1000000, nextTick = 0, lastShownAt = 0, lastPresentRet = 0, lastRefresh = 0;
    int shownGaps = 0, shownGapsLong = 0, presented = 0, shownFps = 0, refreshSamples = 0;
    uint32_t fpsSince = 0;
    double refreshPeriod = 1.0 / 240, paceTime = 1.0 / 59.8261;
    int refreshesPerFrame = 4, ffRefreshes = 1;
    std::vector<double> presentIntervals;
    void* renderer = nullptr;
    auto perfMark = [](int) {};
PRESENT
    DrawJob job;
    for (int i = 0; i < 20; ++i) { clockNow += 2000; presentFrame(job, true); }
    const uint64_t settled = clockNow;
    for (int i = 0; i < 100; ++i) { clockNow += 2000; presentFrame(job, true); }
    // An ignored swap interval is noticed within a few pictures and then paced (not 500 fps);
    // a driver that does hold them, and a refused swap interval, stay at 60.
    assert(clockNow - settled > 1600000 && clockNow - settled < 1900000);
    assert(!swapAccepted || blocks || !swapPaced);
    // The ahead queue owns its deadlines; pace=false must not sleep or advance nextTick.
    const uint64_t before = clockNow;
    nextTick = clockNow + 100000;
    const uint64_t deadline = nextTick;
    driverBlocks = false;
    presentFrame(job, false);
    assert(clockNow - before < 100 && nextTick == deadline);
    // Fast forward still runs multiple emulation frames per presentation; ffRefreshes is
    // the existing adaptive presentation interval, and release returns to one interval.
    ffRefreshes = 2;
    presentFrame(job, true);
    uint64_t gap = nextTick - clockNow;
    assert(gap > 30000 && gap < 34000);
    ffRefreshes = 1;
    presentFrame(job, true);
    gap = nextTick - clockNow;
    assert(gap > 15000 && gap < 18000);
    // A stall is not repaid with an unthrottled burst.
    clockNow += 1000000;
    presentFrame(job, true);
    assert(nextTick > clockNow && nextTick - clockNow < 18000);
}
int main() {
    Run(false, true, true);
    Run(true, true, true);
    Run(false, true, false);
    Run(false, false, false);
}
'''.replace('PRESENT', present)
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'pacing.cpp'
            exe = Path(folder) / 'pacing'
            cpp.write_text(harness)
            subprocess.run(['c++', '-std=c++17', str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
