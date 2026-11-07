#pragma once
#include "SampleAudio.h"
namespace chordharp {
class SampleChords {
    std::array<SampleVoice,32> voices{};const SampleAudio* lastSound=nullptr;int lastChord=-1,lastRoot=-1;bool lastGate=false,lastWhole=false,lastLoop=false;
    double rate=48000;float smoothedGain=1,coefficient=.001f;int latency=16,index=0;std::array<std::array<float,2>,33> alignment{};
public:
    const SampleAudio* sound=nullptr;int rootNote=60;bool wholeChord=false,loop=false;float gain=1,release=.25f;
    void prepare(double r,int delay){rate=r;coefficient=static_cast<float>(1-std::exp(-1/(rate*.02)));smoothedGain=gain;latency=std::clamp(delay,0,32);reset();}
    void reset(){voices={};lastSound=nullptr;lastGate=false;lastChord=-1;alignment={};index=0;}
    void chord(int root,int quality,bool gate,int octave) {
        const int chord=root+quality*12+48*octave;
        const bool changed=lastSound!=sound || chord!=lastChord || rootNote!=lastRoot || wholeChord!=lastWhole || loop!=lastLoop;
        if(changed || (!gate && lastGate))for(auto& voice:voices)if(voice.group<0)voice.stop(release);
        if(gate && sound && (!lastGate || changed)) {
            static constexpr int intervals[4][4]{{0,4,7,12},{0,3,7,12},{0,4,7,10},{0,4,7,11}};
            const int count=wholeChord?1:(quality>=2?4:3);
            for(int n=0;n<count;++n) {
                auto* target=&voices[0];for(auto& voice:voices){if(!voice.audio){target=&voice;break;}if(voice.envelope<target->envelope)target=&voice;}
                const int note=48+root+12*octave+(wholeChord?0:intervals[std::clamp(quality,0,3)][n]);
                target->start(sound,rate,std::exp2(static_cast<float>(note-rootNote)/12),wholeChord?1.f:.45f,loop);target->group=-1;
            }
        }
        lastGate=gate;lastSound=sound;lastChord=chord;lastRoot=rootNote;lastWhole=wholeChord;lastLoop=loop;
    }
    void noteOn(int note,int source,float velocity,int octave) {
        if(!sound)return;noteOff(source);
        auto* target=&voices[0];for(auto& voice:voices){if(!voice.audio){target=&voice;break;}if(voice.envelope<target->envelope)target=&voice;}
        const float pitch=std::exp2(static_cast<float>(std::clamp(note+12*octave,0,127)-rootNote)/12);
        target->start(sound,rate,pitch,std::clamp(velocity,0.f,1.f)*.65f,loop);target->group=source;
    }
    void noteOff(int source){for(auto& voice:voices)if(voice.audio && voice.group==source)voice.stop(release);}
    void releaseAll(){for(auto& voice:voices)voice.stop(release);lastGate=false;}
    std::array<float,2> next() {
        smoothedGain+=(gain-smoothedGain)*coefficient;
        std::array<float,2> out{};for(auto& voice:voices)if(voice.audio){const auto a=voice.next(rate);out[0]+=a[0]*smoothedGain;out[1]+=a[1]*smoothedGain;}
        alignment[static_cast<size_t>(index)]=out;
        const auto result=alignment[static_cast<size_t>((index+33-latency)%33)];index=(index+1)%33;return result;
    }
};
}
