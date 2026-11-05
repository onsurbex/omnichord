#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Original synthesis, not a sampled or measured hardware emulation.
namespace chordharp {
constexpr double pi=3.14159265358979323846;
inline double hz(int note) { return 440.0*std::pow(2.0,(note-69)/12.0); }
inline int chordNote(int root,int quality,int index) {
    constexpr int intervals[3][4]{{0,4,7,12},{0,3,7,12},{0,4,7,10}};
    quality=std::clamp(quality,0,2); index=std::max(0,index);
    const int size=quality==2?4:3;
    return 48+std::clamp(root,0,11)+intervals[quality][index%size]+12*(index/size);
}
class Engine {
    struct Voice { double phase=0,freq=0; float envelope=0,attack=0,velocity=0; };
    std::array<Voice,32> voices{};
    std::array<double,4> padPhase{};
    double sampleRate=44100,lfo=0;
    float padEnvelope=0,smoothedGain=0,filteredHarp=0,filteredPad=0,smoothedHarp=0,smoothedPad=0;
    int root=0,quality=0;
    bool padOn=false;
public:
    float tone=0.35f,sustain=1.6f,vibrato=0.12f,gain=0.65f;
    float harpGain=1,organGain=1,organTone=0.4f,attackSeconds=0.002f;
    int organSound=0,harpSound=0,organOctave=0;
    void prepare(double rate) { sampleRate=std::max(8000.0,rate); reset(); }
    void reset() {
        voices={}; padPhase={}; lfo=0; padEnvelope=0; smoothedGain=0;
        filteredHarp=filteredPad=smoothedHarp=smoothedPad=0; padOn=false;
    }
    void chord(int r,int q,bool on) { root=std::clamp(r,0,11); quality=std::clamp(q,0,2); padOn=on; }
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
        padEnvelope+=((padOn?1.0f:0.0f)-padEnvelope)*coefficient(padOn?0.04f:0.12f);
        float pad=0;
        const int size=quality==2?4:3;
        for(int n=0;n<size;++n) {
            // The organ is now one octave above v1 (which subtracted 12 here).
            const double frequency=hz(chordNote(root,quality,n)+12*std::clamp(organOctave,-1,1));
            padPhase[n]+=frequency*modulation/sampleRate; padPhase[n]-=std::floor(padPhase[n]);
            const double p=2*pi*padPhase[n];
            double wave=std::sin(p);
            auto partial=[&](int harmonic,double amount) {
                if(frequency*harmonic<sampleRate*0.45) wave+=amount*std::sin(harmonic*p);
            };
            if(organSound==1) { partial(3,0.35*organTone); partial(5,0.14*organTone); }
            else if(organSound==2) { partial(2,0.4*organTone); partial(4,0.16*organTone); }
            else if(organSound==3) for(int h=2;h<=5;++h) partial(h,0.5*organTone/h);
            else partial(2,0.12+0.12*organTone);
            pad+=static_cast<float>(wave);
        }
        smoothedHarp+=(harpGain-smoothedHarp)*coefficient(0.01f);
        smoothedPad+=(organGain-smoothedPad)*coefficient(0.01f);
        const auto filter=[&](float value,float& history,float cutoff) {
            const auto c=static_cast<float>(1-std::exp(-2*pi*std::min<double>(cutoff,sampleRate*0.4)/sampleRate));
            history+=c*(value-history); return history;
        };
        const float h=filter(harp*smoothedHarp,filteredHarp,1800+tone*6500);
        const float o=filter(pad*padEnvelope*0.035f*smoothedPad,filteredPad,900+organTone*7000);
        smoothedGain+=(gain-smoothedGain)*coefficient(0.01f);
        return std::tanh((h+o)*smoothedGain);
    }
private:
    float coefficient(float seconds) const { return static_cast<float>(1-std::exp(-1.0/(sampleRate*seconds))); }
};
}
