#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "CircularRead.h"

namespace chordharp {
class StereoDelay {
public:
    float milliseconds=350,feedback=0.3f,mix=0;
    bool synced=false;
    void prepare(double rate) {
        sampleRate=std::max(8000.0,rate);
        smoothing=1-std::exp(-1.0/(sampleRate*0.03));
        for(auto& channel:buffer) channel.assign(static_cast<size_t>(sampleRate*13.7)+4,0);
        reset();
        delaySamples=std::clamp(milliseconds,6.0f,12000.0f)*sampleRate/1000;
    }
    void reset() {
        for(auto& channel:buffer) std::fill(channel.begin(),channel.end(),0);
        write=0; filteredLeft=filteredRight=0;
        wet=std::clamp(mix,0.0f,1.0f); amount=std::clamp(feedback,0.0f,0.85f);
    }
    void process(float& left,float& right) {
        if(buffer[0].empty()) return;
        const double target=std::clamp(milliseconds,6.0f,12000.0f)*sampleRate/1000;
        delaySamples+=(target-delaySamples)*smoothing;
        const float a=read(0,delaySamples),b=read(1,delaySamples*(synced?1.0:1.13));
        filteredLeft+=0.3f*(a-filteredLeft); filteredRight+=0.3f*(b-filteredRight);
        amount+=static_cast<float>(smoothing)*(std::clamp(feedback,0.0f,0.85f)-amount);
        wet+=static_cast<float>(smoothing)*(std::clamp(mix,0.0f,1.0f)-wet);
        buffer[0][write]=left+filteredRight*amount;
        buffer[1][write]=right+filteredLeft*amount;
        left+=a*wet; right+=b*wet;
        if(++write>=buffer[0].size()) write=0;
    }
private:
    float read(size_t channel,double delay) const {
        const double position=circularReadPosition(write,delay,buffer[channel].size());
        const auto first=static_cast<size_t>(position);
        const auto second=(first+1)%buffer[channel].size();
        const float fraction=static_cast<float>(position-static_cast<double>(first));
        return buffer[channel][first]+fraction*(buffer[channel][second]-buffer[channel][first]);
    }
    std::array<std::vector<float>,2> buffer;
    size_t write=0;
    double sampleRate=44100,delaySamples=15435,smoothing=0.001;
    float filteredLeft=0,filteredRight=0,wet=0,amount=0;
};
}
