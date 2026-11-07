#pragma once
#include "SampleAudio.h"
namespace chordharp {
class SampleChords {
    std::array<SampleVoice,16> voices{};const SampleAudio* lastSound=nullptr;int lastChord=-1;bool lastGate=false;
    double rate=48000;int latency=16,index=0;std::array<std::array<float,2>,33> alignment{};
public:
    const SampleAudio* sound=nullptr;int rootNote=60;bool wholeChord=false,loop=false;float gain=1,release=.25f;
    void prepare(double r,int delay){rate=r;latency=std::clamp(delay,0,32);reset();}
    void reset(){voices={};lastSound=nullptr;lastGate=false;lastChord=-1;alignment={};index=0;}
    void chord(int root,int quality,bool gate,int octave) {
        const int chord=root+quality*12+48*octave;
        if(lastSound!=sound || (!gate && lastGate) || (gate && chord!=lastChord))for(auto& voice:voices)voice.stop(release);
        if(gate && sound && (!lastGate || lastSound!=sound || chord!=lastChord)) {
            static constexpr int intervals[4][4]{{0,4,7,12},{0,3,7,12},{0,4,7,10},{0,4,7,11}};
            const int count=wholeChord?1:(quality>=2?4:3);
            for(int n=0;n<count;++n) {
                auto* target=&voices[0];for(auto& voice:voices){if(!voice.audio){target=&voice;break;}if(voice.envelope<target->envelope)target=&voice;}
                const int note=48+root+12*octave+(wholeChord?0:intervals[std::clamp(quality,0,3)][n]);
                target->start(sound,rate,std::exp2(static_cast<float>(note-rootNote)/12),wholeChord?1.f:.45f,loop);
            }
        }
        lastGate=gate;lastSound=sound;lastChord=chord;
    }
    std::array<float,2> next() {
        std::array<float,2> out{};for(auto& voice:voices)if(voice.audio){const auto a=voice.next(rate);out[0]+=a[0]*gain;out[1]+=a[1]*gain;}
        alignment[static_cast<size_t>(index)]=out;
        const auto result=alignment[static_cast<size_t>((index+33-latency)%33)];index=(index+1)%33;return result;
    }
};
}
