#pragma once
#include "FM.h"
#include "SynthFilter.h"

namespace chordharp {
constexpr double pi=3.14159265358979323846;
inline double hz(int note) {return 440*std::pow(2.,(note-69)/12.);}
inline int chordNote(int root,int quality,int index) {
    constexpr int intervals[4][4]{{0,4,7,12},{0,3,7,12},{0,4,7,10},{0,4,7,11}};
    quality=std::clamp(quality,0,3);index=std::max(0,index);const int size=quality>=2?4:3;
    return 48+std::clamp(root,0,11)+intervals[quality][index%size]+12*(index/size);
}
class Engine {
    struct Voice {
        FMVoice fm;std::array<float,7> phase{};float frequency=440,envelope=0,attack=0,velocity=0,activity=0;
        int source=-1,autoGate=0,model=0;bool active=false,retiring=false;float retireGain=1;
    };
    std::array<Voice,32> harp{};std::array<FMVoice,4> organ{};
    std::array<std::array<float,7>,4> organPhase{};
    std::array<float,4> organHz{};int hzRoot=-1,hzQuality=-1,hzOctave=-2;
    FMRuntime organFM,harpFM;Downsample2 downsample;
    DCBlocker harpDC,organDC;int voiceLimit=32;
    double rate=96000;float inverseRate=1.f/96000,lfo=0,padEnvelope=0,smoothedHarp=0,smoothedOrgan=0,smoothedGain=0;
    float legacyDecay=.999f,attackCoefficient=.01f,padAttack=.001f,padRelease=.001f,gainCoefficient=.001f;
    int root=0,quality=0,controlTick=0,activeOrganModel=0,oversampling=2;
    float modelOrganGain=1;bool padOn=false;
    float coefficient(float time) const {return static_cast<float>(1-std::exp(-1/(rate*time)));}
    void updateOrganFrequencies() {
        if(root==hzRoot && quality==hzQuality && organOctave==hzOctave)return;
        hzRoot=root;hzQuality=quality;hzOctave=organOctave;
        for(int i=0;i<4;++i)organHz[static_cast<size_t>(i)]=static_cast<float>(hz(chordNote(root,quality,i)+12*std::clamp(organOctave,-1,1)));
    }
    void triggerOrgan() {
        const int count=quality>=2?4:3;
        updateOrganFrequencies();
        for(int i=0;i<4;++i) {
            if(i<count) organ[static_cast<size_t>(i)].trigger(organHz[static_cast<size_t>(i)],1);
            else organ[static_cast<size_t>(i)]={};
        }
    }
public:
    float tone=.35f,sustain=1.6f,vibrato=.12f,gain=.65f,harpGain=1,organGain=1,organTone=.4f,attackSeconds=.002f;
    int organModel=0,harpSound=0,organOctave=0;
    FMPatch organPatch{},harpPatch{};
    Unison organUnison,harpUnison;
    SynthFilter organFilter,harpFilter;
    float leftOutput=0,rightOutput=0;
    int latencySamples() const {return 32/oversampling;}
    void configure() {
        organFM.configure(organPatch,organTone);harpFM.configure(harpPatch,tone,std::clamp(sustain/.8f,.1f,8.f),attackSeconds);updateOrganFrequencies();
        legacyDecay=static_cast<float>(std::exp(-1/(rate*std::clamp(sustain,.15f,5.f))));
        attackCoefficient=coefficient(std::clamp(attackSeconds,.001f,.08f));padAttack=coefficient(.04f);padRelease=coefficient(.12f);gainCoefficient=coefficient(.01f);
        voiceLimit=std::min(32,112/std::clamp(harpUnison.voices,1,7));
        for(size_t i=static_cast<size_t>(voiceLimit);i<harp.size();++i)harp[i].retiring=true;
    }
    void prepare(double sampleRate) {
        oversampling=sampleRate>=88200?1:2;rate=std::max(8000.,sampleRate)*oversampling;inverseRate=static_cast<float>(1/rate);
        configure();organUnison.prepare(rate);harpUnison.prepare(rate);
        organFilter.prepare(rate);harpFilter.prepare(rate);organDC.prepare(rate);harpDC.prepare(rate);reset();
    }
    void reset() {
        harp={};organ={};organPhase={};padOn=false;padEnvelope=0;lfo=0;controlTick=0;
        smoothedHarp=smoothedOrgan=smoothedGain=0;leftOutput=rightOutput=0;modelOrganGain=1;
        activeOrganModel=std::clamp(organModel,0,4);
        organFM.prepare(rate);harpFM.prepare(rate);organFilter.reset();harpFilter.reset();organDC.reset();harpDC.reset();downsample.reset();
    }
    void chord(int r,int q,bool on) {
        r=std::clamp(r,0,11);q=std::clamp(q,0,3);
        if(on && (!padOn || r!=root || q!=quality)) {root=r;quality=q;triggerOrgan();}
        if(!on && padOn) for(auto& v:organ) v.gate=false;
        root=r;quality=q;padOn=on;
    }
    void pluck(int note,float velocity=.8f,int source=-1) {
        if(note<0 || note>127 || hz(note)>=rate*.225) return;
        Voice* voice=&harp[0];
        for(int i=0;i<voiceLimit;++i) {auto& candidate=harp[static_cast<size_t>(i)];if(!candidate.active) {voice=&candidate;break;}if(candidate.activity<voice->activity)voice=&candidate;}
        *voice={};voice->active=true;voice->model=std::clamp(harpSound,0,1);voice->frequency=static_cast<float>(hz(note));voice->envelope=1;voice->velocity=std::clamp(velocity,0.f,1.f);
        float hold=.08f;for(int i=0;i<6;++i)if(fmCarrier(harpPatch.algorithm,i))hold=std::max(hold,1.2f*std::max(attackSeconds,harpPatch.op[static_cast<size_t>(i)].attack));
        voice->source=source;voice->autoGate=source<0?static_cast<int>(rate*hold):-1;voice->activity=1;
        voice->fm.trigger(voice->frequency,voice->velocity);
        for(size_t u=1;u<7;++u) voice->phase[u]=std::fmod(static_cast<float>(u)*.618034f,1.f);
    }
    void releaseHarp(int source) {for(auto& v:harp) if(v.active && v.source==source) v.fm.gate=false;}
    float next() {
        for(int oversample=0;oversample<oversampling;++oversample) {
            if(++controlTick>=16) {controlTick=0;organFM.tick();harpFM.tick();organUnison.tick();harpUnison.tick();organFilter.tick();harpFilter.tick();}
            lfo+=5.4f*inverseRate;if(lfo>=1)lfo-=1;const float pitch=1+vibrato*.0025f*sine(lfo);
            const float fade=static_cast<float>(1/(rate*.006));
            if(activeOrganModel!=std::clamp(organModel,0,4)) {modelOrganGain=std::max(0.f,modelOrganGain-fade);if(modelOrganGain==0){activeOrganModel=std::clamp(organModel,0,4);if(padOn)triggerOrgan();}}
            else modelOrganGain=std::min(1.f,modelOrganGain+fade);
            float hl=0,hr=0,ol=0,orr=0;
            for(auto& v:harp) {
                if(!v.active)continue;
                if(v.retiring) {v.retireGain=std::max(0.f,v.retireGain-fade);if(v.retireGain==0){v.active=false;continue;}}
                if(v.model==0) {
                    v.attack+=(1-v.attack)*attackCoefficient;
                    for(size_t u=0;u<7;++u) {
                        if(harpUnison.weight[u]<1e-5f)continue;
                        auto& phase=v.phase[u];phase+=v.frequency*pitch*harpUnison.ratio[u]*inverseRate;phase-=static_cast<int>(phase);
                        float sample=sine(phase);
                        if(v.frequency*2<rate*.225)sample+=.28f*tone*sine(phase*2);
                        if(v.frequency*3<rate*.225)sample+=.12f*tone*sine(phase*3);
                        sample*=v.envelope*v.attack*v.velocity*v.retireGain*.18f;hl+=sample*harpUnison.left[u];hr+=sample*harpUnison.right[u];
                    }
                    v.envelope*=legacyDecay;v.activity=v.envelope;if(v.envelope<1e-5f)v.active=false;
                } else {
                    if(v.autoGate>0 && --v.autoGate==0)v.fm.gate=false;
                    const auto audio=v.fm.next(harpFM,harpUnison,inverseRate,pitch);
                    hl+=audio[0]*.2f*v.retireGain;hr+=audio[1]*.2f*v.retireGain;v.active=v.fm.live;v.activity=0;
                    for(const auto& e:v.fm.eg)v.activity=std::max(v.activity,e.value*v.velocity);
                }
            }
            const int count=quality>=2?4:3;
            padEnvelope+=((padOn?1.f:0.f)-padEnvelope)*(padOn?padAttack:padRelease);
            if(activeOrganModel==3) {
                for(int i=0;i<count;++i) {
                    auto& v=organ[static_cast<size_t>(i)];
                    // Octave transposition follows automation without rebuilding the chord.
                    const auto frequency=organHz[static_cast<size_t>(i)];
                    v.frequency=frequency;const auto audio=v.next(organFM,organUnison,inverseRate,pitch);
                    ol+=audio[0]*.085f;orr+=audio[1]*.085f;
                }
            } else if(activeOrganModel<3) {
                if(padEnvelope>1e-7f)for(int i=0;i<count;++i) for(size_t u=0;u<7;++u) {
                    if(organUnison.weight[u]<1e-5f)continue;
                    const auto freq=organHz[static_cast<size_t>(i)];
                    auto& phase=organPhase[static_cast<size_t>(i)][u];phase+=freq*pitch*organUnison.ratio[u]*inverseRate;phase-=static_cast<int>(phase);
                    float wave=sine(phase);
                    const auto harmonic=[&](int h,float amount){if(freq*h<rate*.225)wave+=amount*sine(phase*h);};
                    if(activeOrganModel==1){harmonic(3,.45f*organTone);harmonic(5,.22f*organTone);harmonic(7,.1f*organTone);}
                    else if(activeOrganModel==2){harmonic(2,.25f*organTone);harmonic(4,.1f*organTone);}
                    else{harmonic(2,.18f+.24f*organTone);harmonic(3,.12f*organTone);harmonic(4,.08f*organTone);}
                    wave*=padEnvelope*.05f;ol+=wave*organUnison.left[u];orr+=wave*organUnison.right[u];
                }
            }
            smoothedHarp+=(harpGain-smoothedHarp)*gainCoefficient;smoothedOrgan+=(organGain-smoothedOrgan)*gainCoefficient;smoothedGain+=(gain-smoothedGain)*gainCoefficient;
            const float l=harpFilter.next(harpDC.next(hl*smoothedHarp,0),0)+organFilter.next(organDC.next(ol*smoothedOrgan*modelOrganGain,0),0);
            const float r=harpFilter.next(harpDC.next(hr*smoothedHarp,1),1)+organFilter.next(organDC.next(orr*smoothedOrgan*modelOrganGain,1),1);
            downsample.push(std::tanh(l*smoothedGain),std::tanh(r*smoothedGain));
        }
        const auto output=downsample.output();leftOutput=output[0];rightOutput=output[1];return .5f*(leftOutput+rightOutput);
    }
};
}
