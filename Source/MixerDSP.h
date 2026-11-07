#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace chordharp {
using Stereo=std::array<float,2>;
inline Stereo stereoWidth(Stereo x,float width) {
    const float mid=(x[0]+x[1])*.5f,side=(x[0]-x[1])*.5f*width;
    return {mid+side,mid-side};
}
inline Stereo stereoPan(Stereo x,float pan) {
    // Fold both sides toward the selected speaker, with constant power for
    // a centred mono source. A hard pan never simply discards one input side.
    const float p=std::clamp(pan,-1.f,1.f),scale=1/std::sqrt(1+p*p);
    return {(x[0]*(1-std::max(0.f,p))+x[1]*std::max(0.f,-p))*scale,
            (x[1]*(1-std::max(0.f,-p))+x[0]*std::max(0.f,p))*scale};
}
class MixerDSP {
public:
    struct Channel {bool enabled=true,solo=false;float gain=1,pan=0,delay=0,reverb=0;};
    struct Return {bool enabled=true,solo=false;float gain=.7f,width=1;};
    std::array<Channel,5> channels{};std::array<Return,2> returns{};
    bool masterEnabled=true,fxEnabled=true;float master=0.65f,width=1;
    struct Feeds {Stereo dry{},delay{},reverb{};};
    std::array<float,8> levels{};
    void prepare(double rate) {
        coefficient=static_cast<float>(1-std::exp(-1/(rate*.02)));rampLength=std::max(1,static_cast<int>(rate*.02));levels={};
        for(size_t i=0;i<5;++i){gain[i]=channels[i].enabled?channels[i].gain:0;pan[i]=channels[i].pan;sends[i]={channels[i].delay,channels[i].reverb};}
        for(size_t i=0;i<2;++i){returnGain[i]=returns[i].enabled && fxEnabled?returns[i].gain:0;returnWidth[i]=returns[i].width;}
        masterGain=masterTarget=masterEnabled?master:0;masterLeft=0;masterWidth=width;drySolo=1;
    }
    void beginBlock() {levels={};}
    Feeds feed(const std::array<Stereo,5>& inputs) {
        bool sourceSolo=false,returnSolo=false;
        for(const auto& c:channels)sourceSolo=sourceSolo || (c.enabled && c.solo);
        for(const auto& r:returns)returnSolo=returnSolo || (r.enabled && r.solo && fxEnabled);
        smooth(drySolo,returnSolo?0.f:1.f);Feeds out;
        for(size_t i=0;i<5;++i) {
            const auto& c=channels[i];smooth(gain[i],c.enabled && (!sourceSolo || c.solo)?c.gain:0.f);smooth(pan[i],c.pan);
            smooth(sends[i][0],c.delay);smooth(sends[i][1],c.reverb);
            auto audio=stereoPan(inputs[i],pan[i]);for(auto& x:audio)x*=gain[i];
            levels[i]=std::max(levels[i],std::max(std::abs(audio[0]),std::abs(audio[1])));
            for(size_t s=0;s<2;++s){out.dry[s]+=audio[s]*drySolo;out.delay[s]+=audio[s]*sends[i][0];out.reverb[s]+=audio[s]*sends[i][1];}
        }
        return out;
    }
    Stereo sumReturns(Stereo dry,const std::array<Stereo,2>& wet) {
        const bool solo=(returns[0].enabled && returns[0].solo) || (returns[1].enabled && returns[1].solo);
        for(size_t i=0;i<2;++i) {
            const auto& r=returns[i];smooth(returnGain[i],fxEnabled && r.enabled && (!solo || r.solo)?r.gain:0.f);smooth(returnWidth[i],r.width);
            auto audio=stereoWidth(wet[i],returnWidth[i]);for(auto& x:audio)x*=returnGain[i];
            levels[5+i]=std::max(levels[5+i],std::max(std::abs(audio[0]),std::abs(audio[1])));
            dry[0]+=audio[0];dry[1]+=audio[1];
        }
        return dry;
    }
    Stereo output(Stereo afterTape) {
        smooth(masterWidth,width);afterTape=stereoWidth(afterTape,masterWidth);
        const float target=masterEnabled?master:0.f;
        if(target!=masterTarget){masterTarget=target;masterLeft=rampLength;masterStep=(target-masterGain)/static_cast<float>(masterLeft);}
        if(masterLeft>0){masterGain+=masterStep;if(--masterLeft==0)masterGain=masterTarget;}
        for(auto& x:afterTape)x=std::tanh(x*masterGain);
        levels[7]=std::max(levels[7],std::max(std::abs(afterTape[0]),std::abs(afterTape[1])));return afterTape;
    }
private:
    std::array<float,5> gain{},pan{};std::array<std::array<float,2>,5> sends{};
    std::array<float,2> returnGain{},returnWidth{};float masterGain=.65f,masterWidth=1,drySolo=1,coefficient=.001f;
    float masterTarget=.65f,masterStep=0;int masterLeft=0,rampLength=960;
    void smooth(float& value,float target){value+=coefficient*(target-value);if(std::abs(value-target)<1e-7f)value=target;}
};
}
