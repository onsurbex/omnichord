#pragma once
#include "FMAlgorithms.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace chordharp {
inline constexpr double tau=6.2831853071795864769;
inline const std::array<float,8193> sineTable=[] {
    std::array<float,8193> result{};for(size_t i=0;i<result.size();++i) result[i]=static_cast<float>(std::sin(tau*i/8192.0));return result;
}();
inline float sine(float cycles) {
    const float p=cycles*8192.f;int i=static_cast<int>(p);if(static_cast<float>(i)>p) --i;
    const float fraction=p-static_cast<float>(i);const auto index=static_cast<size_t>(i&8191);
    return sineTable[index]+fraction*(sineTable[index+1]-sineTable[index]);
}
struct FMOperator {
    float ratio=1,fine=0,level=1,attack=.005f,decay=.8f,sustain=.7f,release=.25f,velocity=.4f;
};
struct FMPatch { int algorithm=4,feedback=0;std::array<FMOperator,6> op{}; };
struct FMEnvelopeConfig { float attackStep=1,decayFactor=0,releaseFactor=0,sustain=0;int decaySamples=1,releaseSamples=1; };
struct FMEnvelope {
    float value=0;int stage=0,left=0;bool lastGate=false;
    float next(bool gate,const FMEnvelopeConfig& c) {
        if(gate && !lastGate) stage=1;
        if(!gate && lastGate) {stage=4;left=c.releaseSamples;}
        lastGate=gate;
        if(stage==1) {value=std::min(1.f,value+c.attackStep);if(value>=1) {stage=2;left=c.decaySamples;}}
        else if(stage==2) {value=c.sustain+(value-c.sustain)*c.decayFactor;if(--left<=0) {value=c.sustain;stage=3;}}
        else if(stage==3) value=c.sustain;
        else if(stage==4) {value*=c.releaseFactor;if(--left<=0 || value<1e-7f) {value=0;stage=0;}}
        return value;
    }
};
class FMRuntime {
    FMPatch target{};double rate=96000;float coefficient=.01f,fadeStep=.01f;
    std::array<float,6> targetRatio{};
    std::array<FMEnvelopeConfig,6> targetEnvelope{};
public:
    int algorithm=4,revision=0;float gain=1,index=3,feedback=0;
    std::array<float,6> ratio{},level{},velocity{};
    std::array<FMEnvelopeConfig,6> envelope{};
    void configure(const FMPatch& patch,float colour,float decayScale=1,float attackFloor=0) {
        target=patch;target.algorithm=std::clamp(target.algorithm,0,31);target.feedback=std::clamp(target.feedback,0,7);
        targetIndex=.15f+std::clamp(colour,0.f,1.f)*7.85f;scale=decayScale;attackMinimum=attackFloor;compile();
    }
    void prepare(double sampleRate) {
        rate=sampleRate;coefficient=static_cast<float>(1-std::exp(-16.0/(rate*.02)));fadeStep=static_cast<float>(16/(rate*.006));
        algorithm=target.algorithm;gain=1;index=targetIndex;++revision;compile();apply(true);
    }
    void tick() {
        if(algorithm!=target.algorithm) {gain=std::max(0.f,gain-fadeStep);if(gain>0) return;algorithm=target.algorithm;++revision;apply(true);}
        else {gain=std::min(1.f,gain+fadeStep);apply(false);}
    }
private:
    float targetIndex=3,scale=1,attackMinimum=0;
    void compile() {
        for(size_t i=0;i<6;++i) {
            const auto& o=target.op[i];targetRatio[i]=std::clamp(o.ratio,.125f,32.f)*std::exp2(std::clamp(o.fine,-50.f,50.f)/1200);
            auto& e=targetEnvelope[i];const float a=std::max(std::max(.0005f,attackMinimum),o.attack),d=std::max(.005f,o.decay*scale),r=std::max(.005f,o.release*scale);
            e.attackStep=static_cast<float>(1/(rate*a));e.decayFactor=static_cast<float>(std::exp(-6.907755/(rate*d)));
            e.releaseFactor=static_cast<float>(std::exp(-6.907755/(rate*r)));e.sustain=std::clamp(o.sustain,0.f,1.f);
            e.decaySamples=std::max(1,static_cast<int>(rate*d));e.releaseSamples=std::max(1,static_cast<int>(rate*r));
        }
    }
    void apply(bool snap) {
        const float c=snap?1:coefficient;
        index+=c*(targetIndex-index);
        const float f=target.feedback==0?0.f:.03125f*static_cast<float>(1<<target.feedback);feedback+=c*(f-feedback);
        for(size_t i=0;i<6;++i) {
            const auto& o=target.op[i];ratio[i]+=c*(targetRatio[i]-ratio[i]);
            level[i]+=c*(std::clamp(o.level,0.f,1.f)-level[i]);velocity[i]=std::clamp(o.velocity,0.f,1.f);
            envelope[i]=targetEnvelope[i];
        }
    }
};
struct Unison {
    int voices=1;float detune=6,width=.7f,drift=.15f;
    std::array<float,7> ratio{},left{},right{},weight{},position{},motion{};
    float coefficient=.01f,clockStep=.0001f;
    static float pos(int i,int count) {
        if(count==1) return 0;
        if(count%2) {if(i==0) return 0;return (i%2?-1.f:1.f)*static_cast<float>((i+1)/2)/static_cast<float>(count/2);}
        return (i%2?1.f:-1.f)*static_cast<float>(2*(i/2)+1)/static_cast<float>(count-1);
    }
    void prepare(double rate) {
        coefficient=static_cast<float>(1-std::exp(-16/(rate*.015)));clockStep=static_cast<float>(16/rate);
        for(int i=0;i<7;++i) {weight[static_cast<size_t>(i)]=i<std::clamp(voices,1,7)?1.f:0.f;position[static_cast<size_t>(i)]=i<std::clamp(voices,1,7)?pos(i,std::clamp(voices,1,7)):0;motion[static_cast<size_t>(i)]=i*.137f;}
        tick();
    }
    void tick() {
        const int count=std::clamp(voices,1,7);float power=0;
        for(int i=0;i<7;++i) {const auto k=static_cast<size_t>(i);weight[k]+=coefficient*((i<count?1.f:0.f)-weight[k]);position[k]+=coefficient*(i<count?pos(i,count)-position[k]:0);power+=weight[k]*weight[k];}
        const float normalize=1/std::sqrt(std::max(.01f,power));
        for(int i=0;i<7;++i) {
            const auto k=static_cast<size_t>(i);motion[k]+=(.071f+.029f*i)*clockStep;if(motion[k]>=1) motion[k]-=1;
            const float p=position[k],curve=p*std::sqrt(std::abs(p));
            const float cents=curve*std::clamp(detune,0.f,40.f)+std::clamp(drift,0.f,1.f)*(sine(motion[k])+.35f*sine(motion[k]*1.371f+.23f));
            ratio[k]=std::exp2(cents/1200);const float pan=std::clamp(p*width,-1.f,1.f);
            left[k]=weight[k]*normalize*std::sqrt(.5f*(1-pan));right[k]=weight[k]*normalize*std::sqrt(.5f*(1+pan));
        }
    }
};
struct FMVoice {
    std::array<std::array<float,6>,7> phase{};
    std::array<FMEnvelope,6> eg{};
    std::array<float,7> feedback1{},feedback2{};
    bool live=false,gate=false;float frequency=440,velocity=1;int revision=-1;
    void trigger(float hz,float vel) {
        *this={};frequency=hz;velocity=std::clamp(vel,0.f,1.f);live=gate=true;
        for(size_t u=1;u<7;++u) for(size_t op=0;op<6;++op) phase[u][op]=std::fmod(static_cast<float>(u)*.618034f+static_cast<float>(op)*.137f,1.f);
    }
    std::array<float,2> next(const FMRuntime& bank,const Unison& unison,float inverseRate,float pitch=1) {
        if(!live) return {};
        if(revision!=bank.revision) {
            eg={};feedback1={};feedback2={};revision=bank.revision;
            for(size_t u=0;u<7;++u)for(size_t i=0;i<6;++i)phase[u][i]=std::fmod(static_cast<float>(u)*.618034f*bank.ratio[i],1.f);
        }
        std::array<float,6> amplitude{};bool living=gate;
        for(size_t i=0;i<6;++i) {amplitude[i]=eg[i].next(gate,bank.envelope[i])*bank.level[i]*(1-bank.velocity[i]*(1-velocity));living=living || eg[i].stage!=0;}
        live=living;if(!live) return {};
        switch(bank.algorithm) {
            case 0: return render<0>(bank,unison,inverseRate*pitch,amplitude);
            case 1: return render<1>(bank,unison,inverseRate*pitch,amplitude);
            case 2: return render<2>(bank,unison,inverseRate*pitch,amplitude);
            case 3: return render<3>(bank,unison,inverseRate*pitch,amplitude);
            case 4: return render<4>(bank,unison,inverseRate*pitch,amplitude);
            case 5: return render<5>(bank,unison,inverseRate*pitch,amplitude);
            case 6: return render<6>(bank,unison,inverseRate*pitch,amplitude);
            case 7: return render<7>(bank,unison,inverseRate*pitch,amplitude);
            case 8: return render<8>(bank,unison,inverseRate*pitch,amplitude);
            case 9: return render<9>(bank,unison,inverseRate*pitch,amplitude);
            case 10: return render<10>(bank,unison,inverseRate*pitch,amplitude);
            case 11: return render<11>(bank,unison,inverseRate*pitch,amplitude);
            case 12: return render<12>(bank,unison,inverseRate*pitch,amplitude);
            case 13: return render<13>(bank,unison,inverseRate*pitch,amplitude);
            case 14: return render<14>(bank,unison,inverseRate*pitch,amplitude);
            case 15: return render<15>(bank,unison,inverseRate*pitch,amplitude);
            case 16: return render<16>(bank,unison,inverseRate*pitch,amplitude);
            case 17: return render<17>(bank,unison,inverseRate*pitch,amplitude);
            case 18: return render<18>(bank,unison,inverseRate*pitch,amplitude);
            case 19: return render<19>(bank,unison,inverseRate*pitch,amplitude);
            case 20: return render<20>(bank,unison,inverseRate*pitch,amplitude);
            case 21: return render<21>(bank,unison,inverseRate*pitch,amplitude);
            case 22: return render<22>(bank,unison,inverseRate*pitch,amplitude);
            case 23: return render<23>(bank,unison,inverseRate*pitch,amplitude);
            case 24: return render<24>(bank,unison,inverseRate*pitch,amplitude);
            case 25: return render<25>(bank,unison,inverseRate*pitch,amplitude);
            case 26: return render<26>(bank,unison,inverseRate*pitch,amplitude);
            case 27: return render<27>(bank,unison,inverseRate*pitch,amplitude);
            case 28: return render<28>(bank,unison,inverseRate*pitch,amplitude);
            case 29: return render<29>(bank,unison,inverseRate*pitch,amplitude);
            case 30: return render<30>(bank,unison,inverseRate*pitch,amplitude);
            case 31: return render<31>(bank,unison,inverseRate*pitch,amplitude);
            default:return {};
        }
    }
private:
    template<int Algorithm,int Step>
    void operatorStep(size_t u,float increment,const std::array<float,6>& amplitude,const FMRuntime& bank,float (&bus)[3],float& feedbackOut) {
        constexpr auto flags=fmAlgorithms[Algorithm][Step];constexpr size_t op=5-Step;
        constexpr int input=(flags>>4)&3,output=flags&3;
        float mod=0;
        if constexpr(input!=0)mod=bus[input]*bank.index;
        if constexpr((flags&0x40)!=0)mod+=(feedback1[u]+feedback2[u])*.5f*bank.feedback;
        auto& ph=phase[u][op];const float step=increment*bank.ratio[op];ph+=step;ph-=static_cast<int>(ph);
        const float limit=std::clamp((.49f-step)/.08f,0.f,1.f);
        const float value=sine(ph+mod/static_cast<float>(tau))*amplitude[op]*limit;
        if constexpr((flags&0x80)!=0)feedbackOut=value;
        if constexpr((flags&4)!=0)bus[output]+=value;else bus[output]=value;
        if constexpr(Step<5)operatorStep<Algorithm,Step+1>(u,increment,amplitude,bank,bus,feedbackOut);
    }
    template<int Algorithm>
    std::array<float,2> render(const FMRuntime& bank,const Unison& unison,float speed,const std::array<float,6>& amplitude) {
        float l=0,r=0;constexpr float normalize=1.f/static_cast<float>(fmCarriers(Algorithm));
        const float volume=bank.gain*velocity*normalize;
        for(size_t u=0;u<7;++u) {
            if(unison.weight[u]<1e-5f)continue;
            float bus[3]{},feedbackOut=0;
            operatorStep<Algorithm,0>(u,frequency*speed*unison.ratio[u],amplitude,bank,bus,feedbackOut);
            feedback2[u]=feedback1[u];feedback1[u]=feedbackOut;
            const float sample=bus[0]*volume;l+=sample*unison.left[u];r+=sample*unison.right[u];
        }
        return {l,r};
    }
};
}
