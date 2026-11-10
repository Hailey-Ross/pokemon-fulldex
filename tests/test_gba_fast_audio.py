"""Exercise the player's actual Sound method with mGBA's real audio resampler."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[1]
MGBA = Path(os.environ.get('GBAW_MGBA', str(Path.home()/'pokemon-gba-wide/mgba')))
BUILD = Path(os.environ.get('GBAW_MGBA_BUILD', str(MGBA/'build')))

class GbaFastAudioTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and (BUILD/'libmgba.a').exists(),
                         'built mGBA static library required')
    def test_accelerated_audio_does_not_accumulate(self):
        source = Path(os.environ.get('GBAW_AUDIO_SOURCE', ROOT/'gba/frontend/main.cpp')).read_text()
        start = source.index('    bool ffWas = false;')
        sound = source[start:source.index('    // The game', start)]
        self.assertIn('ffSpeedNow = speed;\n                for (int i = 0; i < run * speed;', source)
        code = r'''
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
using Uint32 = uint32_t;
static Uint32 queued=0, total=0, peak=0;
static Uint32 SDL_GetQueuedAudioSize(int){return queued;}
static void SDL_ClearQueuedAudio(int){queued=0;}
static void SDL_QueueAudio(int,const void*,Uint32 n){queued+=n;total+=n;peak=std::max(peak,queued);}
static mAudioBuffer input;
struct Settings {std::string mute="0";std::string Get(const char*){return mute;}};
struct App {
    mCore* core=nullptr;int audio=1;
    struct {int freq=48000,samples=512;} have;
    const double Fps=60;
    Settings set;mAudioResampler resampler{};mAudioBuffer audioOut{};
    std::vector<int16_t> audioTmp=std::vector<int16_t>(32768);
''' + sound + r'''
};
static void Feed(App& a,bool ff){
    // One GBA frame at 32768 Hz, stereo, with distinct channels.
    int16_t samples[548*2];for(int i=0;i<548;i++){samples[i*2]=1000;samples[i*2+1]=-1000;}
    assert(mAudioBufferWrite(&input,samples,548)==548);a.Sound(true,ff);
}
int main(){
    mCore core{};
    core.getAudioBuffer=[](mCore*){return &input;};
    core.audioSampleRate=[](const mCore*)->unsigned{return 32768;};
    core.frequency=[](const mCore*)->int32_t{return 16777216;};
    core.frameCycles=[](const mCore*)->int32_t{return 280896;};
    for(int speed: {1,2,4,8}){
        App a;a.core=&core;a.ffSpeedNow=speed;
        mAudioBufferInit(&input,16384,2);mAudioBufferInit(&a.audioOut,16384,2);
        mAudioResamplerInit(&a.resampler,mINTERPOLATOR_SINC);
        queued=total=peak=0;
        for(int frame=0;frame<180;frame++){
            for(int i=0;i<speed;i++)Feed(a,speed>1);
            queued=queued>3200?queued-3200:0; // 1/60 second consumed by the device
        }
        printf("speed=%d queued_bytes=%u peak=%u total=%u\n",speed,queued,peak,total);
        assert(total>180*3000 && total<180*3400);
        assert(peak<48000); // bounded latency, not the previous 30-second queue
        a.set.mute="1";SDL_ClearQueuedAudio(1);a.ffWas=false;a.ffSpeedNow=8;
        for(int i=0;i<32;i++)Feed(a,true);assert(queued==0);
        a.set.mute="0";a.Sound(true,false);assert(!a.ffWas);
        // If the device stops consuming audio, the cap still bounds the queue.
        queued=peak=0;a.ffSpeedNow=8;
        for(int i=0;i<800;i++)Feed(a,true);assert(queued<48000);
        a.audio=0;Feed(a,true);assert(mAudioBufferAvailable(&input)==0);
        mAudioResamplerDeinit(&a.resampler);mAudioBufferDeinit(&a.audioOut);mAudioBufferDeinit(&input);
    }
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'audio.cpp';cpp.write_text(code);exe=Path(folder)/'audio'
            # mCore's layout includes directory state in this project's static build.
            subprocess.run(['c++','-std=c++17','-DENABLE_DIRECTORIES','-DENABLE_VFS_FD',
                            '-I'+str(MGBA/'include'),
                            '-I'+str(BUILD/'include'),str(cpp),str(BUILD/'libmgba.a'),
                            '-lm','-lpthread','-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
