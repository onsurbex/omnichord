#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Original synthesis, not a sampled or measured hardware emulation.
namespace chordharp {
constexpr double pi = 3.14159265358979323846;
inline double hz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }
inline int chordNote(int root, int quality, int index) {
    constexpr int intervals[3][4] {{0,4,7,12}, {0,3,7,12}, {0,4,7,10}};
    const int size = quality == 2 ? 4 : 3;
    return 48 + std::clamp(root,0,11) + intervals[std::clamp(quality,0,2)][index % size] + 12 * (index / size);
}
class Engine {
    struct Voice { double phase=0, freq=0; float envelope=0, attack=0, velocity=0; };
    std::array<Voice, 32> voices{};
    std::array<double,4> padPhase{};
    double sampleRate=44100, lfo=0;
    float padEnvelope=0, smoothedGain=0, filtered=0;
    int root=0, quality=0;
    bool padOn=false;
public:
    float tone=0.35f, sustain=1.6f, vibrato=0.12f, gain=0.65f;
    void prepare(double rate) { sampleRate=std::max(8000.0,rate); reset(); }
    void reset() { voices={}; padPhase={}; lfo=0; padEnvelope=0; smoothedGain=0; filtered=0; padOn=false; }
    void chord(int r, int q, bool on) { root=std::clamp(r,0,11); quality=std::clamp(q,0,2); padOn=on; }
    void pluck(int note, float velocity=0.8f) {
        if(note<0 || note>127) return;
        auto* v=&voices[0];
        for(auto& candidate:voices) if(candidate.envelope < v->envelope) v=&candidate;
        *v={0,hz(note),1,0,std::clamp(velocity,0.0f,1.0f)};
    }
    float next() {
        lfo += 5.4/sampleRate; if(lfo>=1) lfo-=1;
        const double modulation=1.0 + vibrato*0.0025*std::sin(2*pi*lfo);
        const float decay=static_cast<float>(std::exp(-1.0/(sampleRate*std::clamp(sustain,0.15f,5.0f))));
        float harp=0;
        for(auto& v:voices) {
            if(v.envelope<0.00001f) continue;
            v.phase+=v.freq*modulation/sampleRate; v.phase-=std::floor(v.phase);
            v.attack += (1-v.attack)*static_cast<float>(1-std::exp(-1.0/(sampleRate*0.002)));
            const double p=2*pi*v.phase;
            // A rounded fundamental and restrained octave partials produce a soft electronic harp.
            double wave=std::sin(p);
            if(v.freq*2 < sampleRate*0.45) wave+=0.28*tone*std::sin(2*p);
            if(v.freq*3 < sampleRate*0.45) wave+=0.12*tone*std::sin(3*p);
            harp+=static_cast<float>(wave)*v.envelope*v.attack*v.velocity*0.13f;
            v.envelope*=decay;
        }
        const float target=padOn ? 1.0f : 0.0f;
        padEnvelope+=(target-padEnvelope)*static_cast<float>(1-std::exp(-1.0/(sampleRate*(padOn?0.04:0.12))));
        float pad=0;
        const int size=quality==2?4:3;
        for(int n=0;n<size;++n) {
            const double frequency=hz(chordNote(root,quality,n)-12);
            padPhase[n]+=frequency*modulation/sampleRate; padPhase[n]-=std::floor(padPhase[n]);
            pad+=static_cast<float>(std::sin(2*pi*padPhase[n])+0.12*std::sin(4*pi*padPhase[n]));
        }
        const float mixed=harp+pad*padEnvelope*0.035f;
        const float cutoff=1800+tone*6500;
        const float coefficient=static_cast<float>(1-std::exp(-2*pi*std::min<double>(cutoff,sampleRate*0.4)/sampleRate));
        filtered+=coefficient*(mixed-filtered);
        smoothedGain+=(gain-smoothedGain)*static_cast<float>(1-std::exp(-1.0/(sampleRate*0.01)));
        return std::tanh(filtered*smoothedGain);
    }
};
}
