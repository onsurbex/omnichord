#pragma once
#include "Engine.h"
#include "MixerDSP.h"

namespace chordharp {
inline constexpr std::array<const char*,10> organNames{{"Clasico","Reed","Suave","Principal","Pleno","Flauta","Tonewheel","Saw Organ","Square Organ","Triangle Organ"}};
class ChordSynth {
    struct Voice {FMVoice fm;std::array<float,7> phase{};float frequency=440,velocity=1,envelope=0,fade=1;int id=-99;bool live=false,gate=false,retiring=false;};
    std::array<Voice,32> voices{};FMRuntime runtime;Downsample2 downsample;DCBlocker dc;
    double rate=96000;float inverse=1.f/96000,attack=.001f,release=.001f,fadeStep=.001f,lfo=0;
    int oversampling=2,tick=0,lastChord=-1,limit=24;bool chordGate=false;
    Voice& allocate() {
        auto* result=&voices[0];
        for(int i=0;i<limit;++i){auto& v=voices[static_cast<size_t>(i)];if(!v.live)return v;if(v.envelope<result->envelope)result=&v;}
        return *result;
    }
    void start(int note,int id,float velocity) {
        if(note<0 || note>127)return;
        auto& v=allocate();v={};v.frequency=static_cast<float>(hz(note));v.velocity=std::clamp(velocity,0.f,1.f);v.id=id;v.live=v.gate=true;
        v.fm.trigger(v.frequency,v.velocity);for(size_t i=1;i<7;++i)v.phase[i]=std::fmod(static_cast<float>(i)*.618034f,1.f);
    }
    float organWave(float phase,float frequency) const {
        float value=0;
        const auto add=[&](int partial,float amount){if(frequency*.5f*partial<rate*.225)value+=amount*sine(phase*static_cast<float>(partial));};
        if(model<3) {
            add(2,1);
            if(model==1){add(6,.45f*colour);add(10,.22f*colour);add(14,.1f*colour);}
            else if(model==2){add(4,.25f*colour);add(8,.1f*colour);}
            else {add(4,.18f+.24f*colour);add(6,.12f*colour);add(8,.08f*colour);}
        } else if(model==6) {
            constexpr int partials[]{1,3,2,4,6,8,10,12,16};constexpr float drawbars[]{.35f,.18f,1.f,.65f,.36f,.28f,.18f,.13f,.09f};
            for(int i=0;i<9;++i)add(partials[i],drawbars[i]*(i<3?1.f:colour));
        } else if(model>=7) {
            for(int h=1;h<=16;++h) {
                if(model!=7 && h%2==0)continue;
                float amplitude=1.f/static_cast<float>(h);
                if(model==9)amplitude=(h%4==1?1.f:-1.f)/static_cast<float>(h*h);
                add(h*2,amplitude*(h==1?1.f:colour));
            }
        } else {
            constexpr float stops[3][8]{{1,.45f,.2f,.18f,.07f,.06f,.03f,.04f},{.8f,.6f,.45f,.35f,.15f,.2f,.1f,.15f},{1,.15f,.04f,.08f,0,.02f,0,.01f}};
            for(int h=1;h<=8;++h)add(h*2,stops[model-3][h-1]*(h==1?1.f:colour));
        }
        return value;
    }
public:
    bool fm=false;int model=0,octave=0;float colour=.4f,vibrato=.12f;
    FMPatch patch{};Unison unison;SynthFilter filter;
    void configure() {
        runtime.configure(patch,colour);limit=std::min(24,84/std::clamp(unison.voices,1,7));
        for(size_t i=static_cast<size_t>(limit);i<voices.size();++i)voices[i].retiring=true;
    }
    void prepare(double sampleRate) {
        oversampling=sampleRate>=88200?1:2;rate=std::max(8000.,sampleRate)*oversampling;inverse=static_cast<float>(1/rate);
        attack=static_cast<float>(1-std::exp(-1/(rate*.025)));release=static_cast<float>(1-std::exp(-1/(rate*.12)));fadeStep=static_cast<float>(1/(rate*.006));
        configure();unison.prepare(rate);filter.prepare(rate);dc.prepare(rate);reset();
    }
    void reset(){voices={};lastChord=-1;chordGate=false;tick=0;lfo=0;runtime.prepare(rate);filter.reset();dc.reset();downsample.reset();}
    void noteOn(int note,int id,float velocity){noteOff(id);start(std::clamp(note+12*octave,0,127),id,velocity);}
    void noteOff(int id){for(auto& v:voices)if(v.live && v.id==id){v.gate=false;v.fm.gate=false;}}
    void releaseAll(){for(auto& v:voices){v.gate=false;v.fm.gate=false;}chordGate=false;}
    void chord(int root,int quality,bool gate) {
        const int key=root+quality*12+48*octave;
        if((!gate && chordGate) || (gate && key!=lastChord))for(auto& v:voices)if(v.id<0){v.gate=false;v.fm.gate=false;}
        if(gate && (!chordGate || key!=lastChord)) {
            for(int i=0;i<(quality>=2?4:3);++i)start(chordNote(root,quality,i)+12*octave,-i-1,1);
        }
        lastChord=key;chordGate=gate;
    }
    Stereo next() {
        for(int n=0;n<oversampling;++n) {
            if(++tick>=16){tick=0;runtime.tick();unison.tick();filter.tick();}
            lfo+=5.4f*inverse;if(lfo>=1)lfo-=1;const float pitch=1+vibrato*.0025f*sine(lfo);Stereo out{};
            for(auto& v:voices)if(v.live) {
                if(v.retiring){v.fade=std::max(0.f,v.fade-fadeStep);if(v.fade==0){v.live=false;continue;}}
                if(fm) {
                    const auto a=v.fm.next(runtime,unison,inverse,pitch);out[0]+=a[0]*.085f*v.fade;out[1]+=a[1]*.085f*v.fade;
                    v.live=v.fm.live;v.envelope=0;for(const auto& e:v.fm.eg)v.envelope=std::max(v.envelope,e.value);
                } else {
                    v.envelope+=((v.gate?1.f:0.f)-v.envelope)*(v.gate?attack:release);
                    if(!v.gate && v.envelope<1e-7f){v.live=false;continue;}
                    for(size_t u=0;u<7;++u)if(unison.weight[u]>1e-5f) {
                        auto& phase=v.phase[u];phase+=v.frequency*.5f*unison.ratio[u]*inverse*pitch;phase-=static_cast<int>(phase);
                        const float audio=organWave(phase,v.frequency)*v.envelope*v.velocity*.05f*v.fade;
                        out[0]+=audio*unison.left[u];out[1]+=audio*unison.right[u];
                    }
                }
            }
            downsample.push(std::tanh(filter.next(dc.next(out[0],0),0)),std::tanh(filter.next(dc.next(out[1],1),1)));
        }
        return downsample.output();
    }
};
}
