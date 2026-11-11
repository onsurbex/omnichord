#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <cstdint>

namespace chordharp {
struct SampleAudio {
    std::array<std::vector<float>,2> channel;
    std::array<std::vector<std::int16_t>,2> pcm16;
    double rate=44100,beats=0;int root=60;float gain=1;
    bool drum=false,sustain=false,openHat=false;
    size_t frames() const {return pcm16[0].empty()?channel[0].size():pcm16[0].size();}
    size_t storageBytes() const {return (channel[0].size()+channel[1].size())*sizeof(float)+(pcm16[0].size()+pcm16[1].size())*sizeof(std::int16_t);}
    void compact(bool sixteenBit) {
        if(channel[0]==channel[1])std::vector<float>().swap(channel[1]);
        if(sixteenBit)for(size_t c=0;c<2;++c){pcm16[c].reserve(channel[c].size());for(float x:channel[c])pcm16[c].push_back(static_cast<std::int16_t>(std::clamp(std::lrint(x*32768.f),-32768l,32767l)));std::vector<float>().swap(channel[c]);}
    }
    float read(size_t c,double position) const {
        const auto index=static_cast<long long>(position);const float t=static_cast<float>(position-index);
        const auto at=[&](long long i){if(i<0 || static_cast<unsigned long long>(i)>=frames())return 0.f;const auto n=static_cast<size_t>(i);if(!pcm16[0].empty())return pcm16[pcm16[c].empty()?0:c][n]/32768.f;return channel[channel[c].empty()?0:c][n];};
        const float a=at(index-1),b=at(index),d=at(index+1),e=at(index+2);
        return b+.5f*t*(d-a+t*(2*a-5*b+4*d-e+t*(3*(b-d)+e-a)));
    }
};
struct SampleVoice {
    const SampleAudio* audio=nullptr;double position=0,speed=1,playbackRate=48000;float gain=1,envelope=0,attackStep=1,releaseStep=1;int group=-1;bool gate=true,loop=false;
    void start(const SampleAudio* sound,double playbackRate,float pitch=1,float velocity=1,bool looping=false) {
        audio=sound;position=0;speed=sound?sound->rate/playbackRate*pitch:1;this->playbackRate=playbackRate;gain=velocity;envelope=0;gate=true;loop=looping;attackStep=static_cast<float>(1/(playbackRate*.0005));
    }
    void stop(float seconds=.04f) {gate=false;releaseStep=envelope/static_cast<float>(std::max(1.,playbackRate*seconds));}
    std::array<float,2> next(double /*rate*/) {
        if(!audio || audio->frames()<4){audio=nullptr;return {};}
        envelope=gate?std::min(1.f,envelope+attackStep):std::max(0.f,envelope-releaseStep);
        if(!gate && envelope<1e-5f){audio=nullptr;return {};}
        const double end=static_cast<double>(audio->frames());
        if(position>=end){audio=nullptr;return {};}
        std::array<float,2> result{audio->read(0,position),audio->read(1,position)};
        if(loop && gate && end>audio->rate*.1) {
            const double start=end*.3,finish=end*.8,cross=std::min(end*.1,audio->rate*.02);
            if(position>finish-cross) {
                const double f=std::clamp((position-(finish-cross))/cross,0.,1.);
                const double other=start+position-(finish-cross);
                for(size_t c=0;c<2;++c)result[c]=static_cast<float>(result[c]*std::cos(f*1.5707963267948966)+audio->read(c,other)*std::sin(f*1.5707963267948966));
            }
            position+=speed;if(position>=finish)position=start+cross+std::fmod(position-finish,std::max(1.,finish-start-cross));
        } else position+=speed;
        for(auto& value:result)value*=gain*envelope;
        return result;
    }
};
}
