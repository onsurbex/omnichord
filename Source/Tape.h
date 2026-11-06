#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace chordharp {
// Original tape-inspired DSP: soft saturation, band limitation, speed modulation and hiss.
class Tape {
    struct Channel { float previous=0,low=0,dc=0,noiseLow=0,noiseHigh=0; };
    std::array<Channel,2> channel{};
    std::array<std::vector<float>,2> buffer;
    std::array<std::uint32_t,2> random{{0x93ac127u,0xbe531af1u}};
    size_t write=0;
    double rate=44100,wowPhase=0,flutterPhase=0;
    float smoothing=0,wet=0,drive=0,noise=0,wow=0,flutter=0;
    float dcCoefficient=0,noiseLowCoefficient=0,noiseHighCoefficient=0;
public:
    bool enabled=false;
    float noiseLevel=0,wowAmount=0.15f,flutterAmount=0.1f,driveAmount=0.25f;
    void prepare(double sampleRate) {
        rate=std::max(8000.0,sampleRate);
        smoothing=coefficient(1.0/0.025,rate);
        dcCoefficient=coefficient(2*pi*25,rate*2);
        noiseLowCoefficient=coefficient(2*pi*700,rate);
        noiseHighCoefficient=coefficient(2*pi*std::min(10000.0,rate*0.4),rate);
        for(auto& samples:buffer) samples.assign(static_cast<size_t>(rate*0.02)+8,0);
        reset();
    }
    void reset() {
        for(auto& samples:buffer) std::fill(samples.begin(),samples.end(),0);
        channel={}; write=0; wowPhase=flutterPhase=0;
        random={{0x93ac127u,0xbe531af1u}};
        wet=enabled?1.0f:0.0f;
        drive=std::clamp(driveAmount,0.0f,1.0f); noise=std::clamp(noiseLevel,0.0f,1.0f);
        wow=std::clamp(wowAmount,0.0f,1.0f); flutter=std::clamp(flutterAmount,0.0f,1.0f);
    }
    void process(float& left,float& right,bool allowNoise=true) {
        if(buffer[0].empty()) return;
        smooth(wet,enabled?1.0f:0.0f); smooth(drive,driveAmount); smooth(noise,noiseLevel);
        smooth(wow,wowAmount); smooth(flutter,flutterAmount);
        wowPhase+=0.38/rate; flutterPhase+=7.3/rate;
        wowPhase-=std::floor(wowPhase); flutterPhase-=std::floor(flutterPhase);
        if(!enabled && wet<0.000001f) {
            buffer[0][write]=left; buffer[1][write]=right;
            channel[0].previous=left; channel[1].previous=right;
            if(++write==buffer[0].size()) write=0;
            return;
        }
        // Common speed variation preserves the stereo image. A 4 ms centre
        // delay gives both oscillators room to move in either direction.
        const double delay=rate*(0.004+0.0025*wow*std::sin(2*pi*wowPhase)
                                      +0.0002*flutter*std::sin(2*pi*flutterPhase));
        const float g=1+10*drive,bias=0.012f*drive;
        const float offset=std::tanh(g*bias);
        const float lowCoefficient=coefficient(2*pi*std::min<double>(14000-5000*drive,rate*0.42),rate*2);
        float* outputs[]{&left,&right};
        for(size_t c=0;c<2;++c) {
            const float input=*outputs[c]; auto& history=channel[c];
            const auto saturate=[&](float x) { return x+drive*((std::tanh(g*(x+bias))-offset)/g-x); };
            // Process at twice the rate, then low-pass before decimation.
            for(const float x:{(history.previous+input)*0.5f,input}) {
                const float shaped=saturate(x);
                history.low+=lowCoefficient*(shaped-history.low);
                history.dc+=dcCoefficient*(history.low-history.dc);
            }
            history.previous=input;
            buffer[c][write]=history.low-history.dc;
            auto& seed=random[c]; seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
            const float white=allowNoise?(static_cast<float>(seed>>8)*(2.0f/16777215.0f)-1):0;
            history.noiseLow+=noiseLowCoefficient*(white-history.noiseLow);
            history.noiseHigh+=noiseHighCoefficient*(white-history.noiseLow-history.noiseHigh);
            const float processed=read(c,delay)+history.noiseHigh*noise*0.008f;
            if(wet>0.000001f) *outputs[c]=input+wet*(processed-input);
        }
        if(++write==buffer[0].size()) write=0;
    }
private:
    static constexpr double pi=3.14159265358979323846;
    static float coefficient(double speed,double sampleRate) { return static_cast<float>(1-std::exp(-speed/sampleRate)); }
    void smooth(float& value,float target) { value+=smoothing*(std::clamp(target,0.0f,1.0f)-value); }
    float read(size_t c,double delay) const {
        double position=static_cast<double>(write)-delay;
        if(position<0) position+=static_cast<double>(buffer[c].size());
        const auto centre=static_cast<size_t>(position); const auto size=buffer[c].size();
        const float t=static_cast<float>(position-static_cast<double>(centre));
        const float a=buffer[c][(centre+size-1)%size],b=buffer[c][centre];
        const float d=buffer[c][(centre+1)%size],e=buffer[c][(centre+2)%size];
        return b+0.5f*t*(d-a+t*(2*a-5*b+4*d-e+t*(3*(b-d)+e-a)));
    }
};
}
