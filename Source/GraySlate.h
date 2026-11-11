#pragma once
#include "SampleAudio.h"
#include "Engine.h"
#include <memory>

namespace chordharp {
struct GraySlateBank {
    std::array<SampleAudio,36> chords;
    std::array<std::array<SampleAudio,12>,36> harps;
    std::array<SampleAudio,12> sevenths;
    struct Pick {const SampleAudio* audio=nullptr;float pitch=1;};
    static int key(int root,int quality) {return quality<0 || quality>2?-1:quality*12+std::clamp(root,0,11);}
    const SampleAudio* chord(int root,int quality) const {
        const int k=key(root,quality==3?0:quality);return k>=0 && chords[static_cast<size_t>(k)].frames()>0?&chords[static_cast<size_t>(k)]:nullptr;
    }
    Pick majorSeventh(int root,int octave) const {
        const auto& sample=sevenths[static_cast<size_t>((std::clamp(root,0,11)+11)%12)];
        return sample.frames()>0?Pick{&sample,std::exp2((48+root+11+12*octave-sample.root)/12.f)}:Pick{};
    }
    Pick step(int root,int quality,int position,int octave=0) const {
        const int k=key(root,quality==3?2:quality);if(k<0)return {};
        const int index=std::max(0,position)%12;const auto& recordings=harps[static_cast<size_t>(k)];
        int selected=index;
        if(recordings[static_cast<size_t>(selected)].frames()==0) {
            int distance=1000;selected=-1;
            for(int i=0;i<12;++i)if(recordings[static_cast<size_t>(i)].frames()>0 && std::abs(i-index)<distance){selected=i;distance=std::abs(i-index);}
            if(selected<0)return {};
        }
        const int shift=12*(octave+std::max(0,position)/12)+chordNote(root,quality,index)-chordNote(root,quality,selected)+(quality==3 && index%3==2?1:0);
        return {&recordings[static_cast<size_t>(selected)],std::exp2(shift/12.f)};
    }
    Pick note(int root,int quality,int midi) const {
        const int k=key(root,quality==3?2:quality);if(k<0)return {};
        const SampleAudio* selected=nullptr;int distance=1000;
        for(const auto& sample:harps[static_cast<size_t>(k)])if(sample.frames()>0 && std::abs(midi-sample.root)<distance){selected=&sample;distance=std::abs(midi-sample.root);}
        return selected?Pick{selected,std::exp2((midi-selected->root)/12.f)}:Pick{};
    }
    size_t storageBytes() const {size_t n=0;for(const auto& s:chords)n+=s.storageBytes();for(const auto& s:sevenths)n+=s.storageBytes();for(const auto& row:harps)for(const auto& s:row)n+=s.storageBytes();return n;}
};

class GraySlatePlayer {
    std::array<SampleVoice,32> voices{};
    double rate=48000;int lastChord=-1,lastOctave=0;bool lastGate=false,lastLoop=true;const GraySlateBank* lastBank=nullptr;
    int delay=16,write=0;std::array<std::array<float,2>,33> alignment{};
    void start(GraySlateBank::Pick pick,int source,float velocity,bool looping) {
        if(!pick.audio)return;
        auto* target=&voices[0];for(auto& v:voices){if(!v.audio){target=&v;break;}if(v.envelope<target->envelope)target=&v;}
        target->start(pick.audio,rate,pick.pitch,velocity*pick.audio->gain,looping);target->group=source;
        target->attackStep=static_cast<float>(1/(rate*std::max(.0005f,attack)));
    }
public:
    const GraySlateBank* bank=nullptr;
    float release=.08f,attack=.0005f;bool loop=true;
    void prepare(double r,int latency){rate=r;delay=std::clamp(latency,0,32);reset();}
    void reset(){voices={};alignment={};write=0;lastChord=-1;lastGate=false;lastBank=nullptr;}
    void releaseAll(){for(auto& v:voices)if(v.audio)v.stop(release);lastGate=false;}
    void noteOff(int source){for(auto& v:voices)if(v.audio && v.group==source)v.stop(release);}
    void chord(int root,int quality,bool gate,int octave) {
        const int chord=root+quality*12;
        const bool changed=chord!=lastChord || octave!=lastOctave || lastBank!=bank || lastLoop!=loop;
        if(changed || (!gate && lastGate))for(auto& v:voices)if(v.audio && v.group<0)v.stop(release);
        if(gate && bank && (!lastGate || changed)) {
            start({bank->chord(root,quality),std::exp2(static_cast<float>(octave))},-1,1,loop);
            if(quality==3)start(bank->majorSeventh(root,octave),-1,.45f,loop);
        }
        lastGate=gate;lastChord=chord;lastOctave=octave;lastBank=bank;lastLoop=loop;
    }
    void pluck(int root,int quality,int position,float velocity=.8f,int source=-2) {if(bank)start(bank->step(root,quality,position),source,velocity,false);}
    void noteOn(int root,int quality,int note,int source,float velocity){noteOff(source);if(bank)start(bank->note(root,quality,note),source,velocity,false);}
    std::array<float,2> next() {
        std::array<float,2> out{};
        for(auto& v:voices)if(v.audio){const auto a=v.next(rate);out[0]+=a[0];out[1]+=a[1];}
        alignment[static_cast<size_t>(write)]=out;const auto result=alignment[static_cast<size_t>((write+33-delay)%33)];write=(write+1)%33;return result;
    }
};
}
