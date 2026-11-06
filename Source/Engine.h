#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "Organ.h"

// Original synthesis, not a sampled or measured hardware emulation.
namespace chordharp {
constexpr double pi=3.14159265358979323846;
inline double hz(int note) { return 440.0*std::pow(2.0,(note-69)/12.0); }
inline int chordNote(int root,int quality,int index) {
    constexpr int intervals[4][4]{{0,4,7,12},{0,3,7,12},{0,4,7,10},{0,4,7,11}};
    quality=std::clamp(quality,0,3); index=std::max(0,index);
    const int size=quality>=2?4:3;
    return 48+std::clamp(root,0,11)+intervals[quality][index%size]+12*(index/size);
}
class Engine {
    struct Voice { double phase=0,freq=0; float envelope=0,attack=0,velocity=0; };
    std::array<Voice,32> voices{};
    std::array<std::array<double,4>,3> padPhase{};
    double sampleRate=44100,lfo=0,rotor=0;
    float percussion=0,keyClick=0,modelGain=1;
    int activeModel=0;
    float padEnvelope=0,smoothedGain=0,filteredHarp=0,filteredPadL=0,filteredPadR=0,smoothedHarp=0,smoothedPad=0,spread=0,chorus=0;
    int root=0,quality=0;
    bool padOn=false;
public:
    float tone=0.35f,sustain=1.6f,vibrato=0.12f,gain=0.65f;
    float harpGain=1,organGain=1,organTone=0.4f,attackSeconds=0.002f;
    int organSound=0,harpSound=0,organOctave=0,organModel=0;
    float organRotary=0.35f,organPercussion=0.5f;
    float organSpread=0.55f,organChorus=0.35f,leftOutput=0,rightOutput=0;
    void prepare(double rate) { sampleRate=std::max(8000.0,rate); reset(); }
    void reset() {
        voices={}; padPhase={}; lfo=0; padEnvelope=0; smoothedGain=0;
        filteredHarp=filteredPadL=filteredPadR=smoothedHarp=smoothedPad=0;
        spread=organSpread; chorus=organChorus; leftOutput=rightOutput=0; padOn=false;
        rotor=0; percussion=keyClick=0; modelGain=1; activeModel=std::clamp(organModel,0,10);
    }
    void chord(int r,int q,bool on) {
        r=std::clamp(r,0,11); q=std::clamp(q,0,3);
        if(on && (!padOn || r!=root || q!=quality)) { percussion=1; keyClick=1; }
        root=r; quality=q; padOn=on;
    }
    void pluck(int note,float velocity=0.8f) {
        if(note<0 || note>127 || hz(note)>=sampleRate*0.45) return;
        auto* v=&voices[0];
        for(auto& candidate:voices) if(candidate.envelope<v->envelope) v=&candidate;
        *v={0,hz(note),1,0,std::clamp(velocity,0.0f,1.0f)};
    }
    float next() {
        lfo+=5.4/sampleRate; if(lfo>=1) lfo-=1;
        const double modulation=1+vibrato*0.0025*std::sin(2*pi*lfo);
        const float decay=static_cast<float>(std::exp(-1.0/(sampleRate*std::clamp(sustain,0.15f,5.0f))));
        const float attack=coefficient(std::clamp(attackSeconds,0.001f,0.08f));
        float harp=0;
        for(auto& v:voices) {
            if(v.envelope<0.00001f) continue;
            v.phase+=v.freq*modulation/sampleRate; v.phase-=std::floor(v.phase);
            v.attack+=(1-v.attack)*attack;
            const double p=2*pi*v.phase;
            double wave=std::sin(p);
            auto partial=[&](int harmonic,double amount) {
                if(v.freq*harmonic<sampleRate*0.45) wave+=amount*std::sin(harmonic*p);
            };
            if(harpSound==1) { partial(2,0.45*tone*v.envelope); partial(5,0.2*tone*v.envelope); }
            else if(harpSound==3) for(int n=2;n<=7;++n) partial(n,0.35*tone/n);
            else if(harpSound!=2) { partial(2,0.28*tone); partial(3,0.12*tone); }
            harp+=static_cast<float>(wave)*v.envelope*v.attack*v.velocity*0.13f;
            v.envelope*=decay;
        }
        const int wantedModel=std::clamp(organModel,0,10);
        const float fadeStep=static_cast<float>(1.0/(sampleRate*0.012));
        if(wantedModel!=activeModel) { modelGain=std::max(0.0f,modelGain-fadeStep); if(modelGain==0) activeModel=wantedModel; }
        else modelGain=std::min(1.0f,modelGain+fadeStep);
        const bool tonewheel=activeModel>=8;
        rotor+=(activeModel==8?0.8:activeModel==9?4.8:6.0)/sampleRate; rotor-=std::floor(rotor);
        percussion*=static_cast<float>(std::exp(-1.0/(sampleRate*0.18)));
        keyClick*=static_cast<float>(std::exp(-1.0/(sampleRate*0.004)));
        padEnvelope+=((padOn?1.0f:0.0f)-padEnvelope)*coefficient(padOn?(tonewheel?0.006f:0.04f):0.12f);
        spread+=(std::clamp(organSpread,0.0f,1.0f)-spread)*coefficient(0.03f);
        chorus+=(std::clamp(organChorus,0.0f,1.0f)-chorus)*coefficient(0.03f);
        float padLeft=0,padRight=0;
        const int size=quality>=2?4:3;
        for(int n=0;n<size;++n) {
            const int note=chordNote(root,quality,n)+12*std::clamp(organOctave,-1,1);
            // A quiet octave rank opens the voicing; detuned side ranks add stereo motion.
            for(int rank=0;rank<3;++rank) {
                const double detune=rank==0?1.0:std::pow(2.0,(rank==1?-1:1)*chorus*7.0/1200.0);
                const double rotary=tonewheel && rank>0?1+organRotary*0.0018*std::sin(2*pi*rotor+(rank==1?0:pi)):1;
                const double frequency=hz(note)*modulation*detune*rotary;
                auto& phase=padPhase[static_cast<size_t>(rank)][static_cast<size_t>(n)];
                phase+=frequency/sampleRate; if(phase>=2) phase-=2;
                if(padEnvelope<0.0000001f) continue;
                const double p=2*pi*phase;
                double wave=std::sin(p);
                auto partial=[&](int harmonic,double amount) {
                    if(frequency*harmonic<sampleRate*0.45) wave+=amount*std::sin(harmonic*p);
                };
                const int legacySound=activeModel>=1 && activeModel<=3?activeModel:organSound;
                if(legacySound==1) { partial(3,0.45*organTone); partial(5,0.22*organTone); partial(7,0.1*organTone); }
                else if(legacySound==2) { partial(2,0.25*organTone); partial(4,0.1*organTone); }
                else if(legacySound==3) for(int h=2;h<=7;++h) partial(h,0.8*organTone/h);
                else { partial(2,0.18+0.24*organTone); partial(3,0.12*organTone); partial(4,0.08*organTone); }
                partial(2,n==1?0.35*spread:0.15*spread);
                if(activeModel>=4) {
                    wave=organRank(activeModel,phase,hz(note),sampleRate,organTone);
                    if(frequency*2<sampleRate*0.45) wave+=0.15*spread*std::sin(2*p);
                    if(tonewheel) {
                        if(frequency*3<sampleRate*0.45) wave+=0.48*organPercussion*percussion*std::sin(3*p);
                        if(frequency*23<sampleRate*.45) wave+=0.07*organPercussion*keyClick*std::sin(23*p);
                        if(activeModel==10) wave=std::tanh(wave*1.6)/1.35;
                    }
                }
                const float sample=static_cast<float>(wave);
                const float side=0.32f*chorus;
                if(rank==0) { padLeft+=sample*(1-side); padRight+=sample*(1-side); }
                else if(rank==1) padLeft+=sample*side;
                else padRight+=sample*side;
            }
        }
        if(tonewheel) {
            const float rotation=0.16f*organRotary*static_cast<float>(std::sin(2*pi*rotor));
            padLeft*=1+rotation; padRight*=1-rotation;
        }
        smoothedHarp+=(harpGain-smoothedHarp)*coefficient(0.01f);
        smoothedPad+=(organGain-smoothedPad)*coefficient(0.01f);
        const auto filter=[&](float value,float& history,float cutoff) {
            const auto c=static_cast<float>(1-std::exp(-2*pi*std::min<double>(cutoff,sampleRate*0.4)/sampleRate));
            history+=c*(value-history); return history;
        };
        const float h=filter(harp*smoothedHarp,filteredHarp,1800+tone*6500);
        const float level=padEnvelope*0.035f*smoothedPad*modelGain;
        const float ol=filter(padLeft*level,filteredPadL,1200+organTone*7000);
        const float oright=filter(padRight*level,filteredPadR,1200+organTone*7000);
        smoothedGain+=(gain-smoothedGain)*coefficient(0.01f);
        leftOutput=std::tanh((h+ol)*smoothedGain);
        rightOutput=std::tanh((h+oright)*smoothedGain);
        return (leftOutput+rightOutput)*0.5f;
    }
private:
    float coefficient(float seconds) const { return static_cast<float>(1-std::exp(-1.0/(sampleRate*seconds))); }
};
}
