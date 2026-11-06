#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

namespace chordharp {
class Ambience {
    struct EQ {
        double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        float process(float x) { const double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return static_cast<float>(y); }
        void configure(int band,float db,double rate) {
            constexpr double pi=3.14159265358979323846;
            const double frequency=std::min(band==0?250.0:band==1?1000.0:5000.0,rate*.4);
            const double w=2*pi*frequency/rate,c=std::cos(w),sn=std::sin(w),A=std::pow(10.0,db/40.0);
            double B0,B1,B2,A0,A1,A2;
            if(band==1) {
                const double alpha=sn/(2*.707);
                B0=1+alpha*A;B1=-2*c;B2=1-alpha*A;A0=1+alpha/A;A1=-2*c;A2=1-alpha/A;
            } else {
                const double beta=2*std::sqrt(A)*sn/std::sqrt(2.0);
                if(band==0) {
                    B0=A*((A+1)-(A-1)*c+beta);B1=2*A*((A-1)-(A+1)*c);B2=A*((A+1)-(A-1)*c-beta);
                    A0=(A+1)+(A-1)*c+beta;A1=-2*((A-1)+(A+1)*c);A2=(A+1)+(A-1)*c-beta;
                } else {
                    B0=A*((A+1)+(A-1)*c+beta);B1=-2*A*((A-1)+(A+1)*c);B2=A*((A+1)+(A-1)*c-beta);
                    A0=(A+1)-(A-1)*c+beta;A1=2*((A-1)-(A+1)*c);A2=(A+1)-(A-1)*c-beta;
                }
            }
            b0=B0/A0;b1=B1/A0;b2=B2/A0;a1=A1/A0;a2=A2/A0;
        }
    };
    juce::Reverb late;
    std::array<std::vector<float>,2> preBuffer,earlyBuffer;
    std::array<std::array<EQ,3>,2> eq;
    size_t preWrite=0,earlyWrite=0;
    double rate=44100,preSamples=0,smoothing=0;
    float wet=0,earlyLevel=0;
    std::array<float,3> eqGain{};
    int eqTick=0;
    static float read(const std::vector<float>& buffer,size_t write,double delay) {
        double position=static_cast<double>(write)-delay;
        if(position<0) position+=static_cast<double>(buffer.size());
        const auto index=static_cast<size_t>(position); const float f=static_cast<float>(position-index);
        return buffer[index]+f*(buffer[(index+1)%buffer.size()]-buffer[index]);
    }
public:
    float roomSize=.5f,damping=.5f,mix=0,predelayMs=0,early=0;
    std::array<float,3> gainDb{};
    void prepare(double sampleRate) {
        rate=std::max(8000.0,sampleRate); late.setSampleRate(rate);
        for(auto& b:preBuffer) b.assign(static_cast<size_t>(rate*.26)+4,0);
        for(auto& b:earlyBuffer) b.assign(static_cast<size_t>(rate*.12)+4,0);
        smoothing=1-std::exp(-1.0/(rate*.02)); reset();
    }
    void reset() {
        late.reset(); preWrite=earlyWrite=0; eqTick=0; preSamples=predelayMs*rate/1000; wet=mix;earlyLevel=early;eqGain=gainDb;
        for(auto& b:preBuffer) std::fill(b.begin(),b.end(),0);
        for(auto& b:earlyBuffer) std::fill(b.begin(),b.end(),0);
        for(auto& channel:eq) for(int i=0;i<3;++i) { channel[static_cast<size_t>(i)]={};channel[static_cast<size_t>(i)].configure(i,gainDb[static_cast<size_t>(i)],rate); }
    }
    void configure() {
        juce::Reverb::Parameters p; p.roomSize=roomSize;p.damping=damping;p.wetLevel=1;p.dryLevel=0;p.width=1;late.setParameters(p);

    }
    void process(float& l,float& r) {
        if(preBuffer[0].empty()) return;
        if(++eqTick>=32) {
            eqTick=0;
            const float step=static_cast<float>(1-std::exp(-32.0/(rate*.025)));
            for(int i=0;i<3;++i) {
                const auto band=static_cast<size_t>(i); const float target=std::clamp(gainDb[band],-12.0f,12.0f);
                if(std::abs(target-eqGain[band])<0.0001f) continue;
                eqGain[band]+=step*(target-eqGain[band]);
                for(auto& channel:eq) channel[band].configure(i,eqGain[band],rate);
            }
        }
        const float dryL=l,dryR=r;
        preSamples+=(std::clamp(predelayMs,0.0f,250.0f)*rate/1000-preSamples)*smoothing;
        preBuffer[0][preWrite]=l;preBuffer[1][preWrite]=r;
        l=read(preBuffer[0],preWrite,preSamples);r=read(preBuffer[1],preWrite,preSamples);
        earlyBuffer[0][earlyWrite]=l;earlyBuffer[1][earlyWrite]=r;
        float reflectionL=0,reflectionR=0;
        constexpr double taps[]{.013,.021,.035,.047,.063};
        constexpr float weights[]{.36f,.29f,.22f,.17f,.12f};
        for(int t=0;t<5;++t) {
            const double delay=taps[t]*rate*(.6+.9*roomSize);
            reflectionL+=read(earlyBuffer[static_cast<size_t>(t%2)],earlyWrite,delay)*weights[t];
            reflectionR+=read(earlyBuffer[static_cast<size_t>((t+1)%2)],earlyWrite,delay*1.09)*weights[t];
        }
        late.processStereo(&l,&r,1);
        earlyLevel+=static_cast<float>(smoothing)*(early-earlyLevel);
        l+=reflectionL*earlyLevel;r+=reflectionR*earlyLevel;
        for(auto& filter:eq[0]) l=filter.process(l);
        for(auto& filter:eq[1]) r=filter.process(r);
        wet+=static_cast<float>(smoothing)*(mix-wet);
        l=dryL+(l-dryL)*wet;r=dryR+(r-dryR)*wet;
        if(++preWrite==preBuffer[0].size()) preWrite=0;
        if(++earlyWrite==earlyBuffer[0].size()) earlyWrite=0;
    }
};
}
