#pragma once
#include "SampleAudio.h"
#include "BeatLoop.h"
#include <cstdint>
#include <limits>

namespace chordharp {
inline constexpr std::array<const char*,7> rhythmNames{{"Omni Pop","Disco","Bossa","Half-time","Shuffle","Waltz","Custom"}};
inline constexpr std::array<std::array<std::uint16_t,5>,6> rhythmPatterns{{
    {{0x0101,0x1010,0x5555,0x0000,0x0000}},{{0x1111,0x1010,0x4444,0x0040,0x0000}},
    {{0x4101,0x1044,0x5555,0x0000,0x2082}},{{0x0001,0x0100,0x5555,0x0000,0x0040}},
    {{0x0101,0x1010,0x9999,0x0040,0x0000}},{{0x0001,0x0110,0x0555,0x0000,0x0000}}
}};
class Rhythm {
    std::array<SampleVoice,24> voices{};double rate=48000,phase=0;std::int64_t nextStep=0;bool lastRun=false;
    bool hostPosition=false,hostTransport=false;double blockPpq=0;int frame=0;
    float smoothedVolume=.7f,volumeCoefficient=.001f;int latency=0,write=0;std::array<std::array<float,2>,33> alignment{};
    BeatLoop loopPlayer;float loopGain=0;bool loopSeek=false;double loopPhase=0;
    double scheduled(std::int64_t step) const {return (static_cast<double>(step)+(step%2!=0?std::clamp(swing,0.f,.6f):0.f))*.25;}
    void seek(double ppq) {nextStep=static_cast<std::int64_t>(std::floor(ppq*4));if(scheduled(nextStep)<ppq-1e-10)++nextStep;}
public:
    bool enabled=false,followHost=true,hostPlaying=false;double bpm=120;int pattern=0;float swing=0,volume=.7f;
    bool useLoop=false,chokeOpenHat=true;const SampleAudio* loopSound=nullptr;
    std::array<std::uint16_t,5> custom{};
    std::array<const SampleAudio*,5> samples{};
    int lastStep=-1;std::array<int,5> hits{};
    void prepare(double sampleRate,int delay=0){rate=sampleRate;volumeCoefficient=static_cast<float>(1-std::exp(-1/(rate*.02)));smoothedVolume=volume;latency=std::clamp(delay,0,32);loopPlayer.prepare(rate);reset();}
    void reset(){alignment={};write=0;voices={};phase=0;nextStep=0;lastRun=false;lastStep=-1;hits={};loopPlayer.reset();loopGain=0;loopPhase=0;loopSeek=false;}
    bool running() const {return enabled && (!hostTransport || !followHost || hostPlaying);}
    void beginBlock(bool hasPpq,double ppq,bool playing,bool hasTransport=false) {
        hostPosition=hasPpq;hostTransport=hasTransport || hasPpq;blockPpq=ppq;hostPlaying=playing;frame=0;
        const bool run=running();
        if(hostPosition && followHost) {
            const double expected=phase;
            if(!lastRun || std::abs(ppq-expected)>std::max(.01,60000/(std::max(20.,bpm)*rate))){seek(ppq);loopSeek=true;}
            phase=ppq;
        } else if(run && !lastRun){phase=0;nextStep=0;loopSeek=true;}
        if(!run && lastRun){for(auto& voice:voices)voice.stop(.005f);lastStep=-1;}
        lastRun=run;
    }
    void trigger(int track,float velocity=1) {
        if(track<0 || track>=5 || !samples[static_cast<size_t>(track)])return;
        if(track==2 && chokeOpenHat)for(auto& voice:voices)if(voice.group==3)voice.stop(.005f);
        auto* target=&voices[0];for(auto& voice:voices){if(!voice.audio){target=&voice;break;}if(voice.envelope<target->envelope)target=&voice;}
        target->start(samples[static_cast<size_t>(track)],rate,1,velocity);target->group=track;++hits[static_cast<size_t>(track)];
    }
    std::array<float,2> next() {
        const bool run=running();
        if(run) {
            const double ppq=(hostPosition && followHost)?blockPpq+frame*std::clamp(bpm,20.,400.)/(60*rate):phase;
            for(int event=0;event<4;++event) {
                if(ppq+1e-10<scheduled(nextStep))break;
                const int length=pattern==5?12:16;const int step=static_cast<int>((nextStep%length+length)%length);lastStep=step;
                const auto& grid=pattern<6?rhythmPatterns[static_cast<size_t>(std::clamp(pattern,0,5))]:custom;
                if(!useLoop)for(size_t track=0;track<5;++track)if((grid[track]&(1u<<step))!=0)trigger(static_cast<int>(track),step%4==0?1.f:.82f);
                ++nextStep;
            }
            if(useLoop)loopPhase=ppq;
            phase=ppq+std::clamp(bpm,20.,400.)/(60*rate);
        }
        ++frame;smoothedVolume+=(volume-smoothedVolume)*volumeCoefficient;std::array<float,2> audio{};
        for(auto& voice:voices)if(voice.audio){const auto a=voice.next(rate);audio[0]+=a[0]*smoothedVolume;audio[1]+=a[1]*smoothedVolume;}
        const float target=run && useLoop && loopSound?1.f:0.f;
        const float ramp=static_cast<float>(1/(rate*.003));loopGain+=std::clamp(target-loopGain,-ramp,ramp);
        if(loopGain>0) {
            const double advance=std::clamp(bpm,20.,400.)/(60*rate);
            const auto a=loopPlayer.next(loopSound,loopPhase,advance,loopSeek);loopSeek=false;loopPhase+=advance;
            audio[0]+=a[0]*loopGain*smoothedVolume;audio[1]+=a[1]*loopGain*smoothedVolume;
        }
        alignment[static_cast<size_t>(write)]=audio;const auto output=alignment[static_cast<size_t>((write+33-latency)%33)];write=(write+1)%33;return output;
    }
};
}
