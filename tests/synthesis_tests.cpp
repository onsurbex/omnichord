#include "Engine.h"
#include "SynthPatches.h"
#include "Rhythm.h"
#include "SampleChords.h"
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
static double fmPower(float modulator,double frequency){
    chordharp::FMPatch p;p.algorithm=4;
    for(auto& o:p.op){o.level=0;o.sustain=1;o.attack=.0005f;}
    p.op[0].level=1;p.op[1].level=modulator;p.op[1].ratio=2;
    chordharp::FMRuntime bank;bank.configure(p,.7f);bank.prepare(48000);
    chordharp::Unison u;u.drift=0;u.prepare(48000);chordharp::FMVoice voice;voice.trigger(440,1);
    double si=0,co=0;
    for(int i=0;i<48000;++i){const auto out=voice.next(bank,u,1.f/48000);if(i>=24000){const auto phase=chordharp::tau*frequency*i/48000;si+=out[0]*std::sin(phase);co+=out[0]*std::cos(phase);}}
    return si*si+co*co;
}
int main(){
    using namespace chordharp;
    require(fmPower(1,1320)>fmPower(0,1320)*100+1,"A modulator must create real FM sidebands");
    require(fmPower(0,440)>100,"A carrier without modulation must retain its fundamental");
    for(double rate:{44100.,48000.,96000.}){
        for(int algorithm=0;algorithm<32;++algorithm){
            Engine e;e.organModel=3;e.organPatch=synthPatch(6,true);e.organPatch.algorithm=algorithm;e.organPatch.feedback=7;
            for(auto& o:e.organPatch.op){o.attack=.001f;o.decay=.04f;o.release=.03f;}
            e.organUnison.voices=7;e.organUnison.detune=40;e.organUnison.drift=1;e.organFilter.resonance=8;e.organFilter.cutoff=2600;
            e.prepare(rate);e.chord(9,3,true);double energy=0;
            for(int i=0;i<static_cast<int>(rate*.04);++i){const auto y=e.next();require(std::isfinite(y)&&std::abs(y)<2,"All 32 algorithms must remain bounded at maximum feedback and seven voices");energy+=y*y;}
            require(energy>1e-6,"Every FM algorithm must have an audible carrier path");
            e.chord(9,3,false);for(int i=0;i<static_cast<int>(rate*.5);++i)e.next();require(std::abs(e.next())<.00001,"Operator release envelopes must end a chord");
            e.reset();require(e.next()==0,"FM panic must clear oscillators, filters and FIR history");
        }
        for(int count=1;count<=7;++count){Engine e;e.organUnison.voices=count;e.organUnison.detune=15;e.prepare(rate);e.chord(0,0,true);double stereo=0;for(int i=0;i<static_cast<int>(rate*.12);++i){e.next();stereo+=std::abs(e.leftOutput-e.rightOutput);}require(count==1?stereo==0:stereo>.001,"Each unison voice count must produce the intended stereo spread");}
    }
    const auto filtered=[](int mode,double frequency){SynthFilter f;f.mode=mode;f.cutoff=1000;f.prepare(48000);double power=0;for(int i=0;i<24000;++i){const float y=f.next(static_cast<float>(std::sin(tau*frequency*i/48000)),0);if(i>=12000)power+=y*y;}return power;};
    require(filtered(0,200)>filtered(0,8000)*100,"LP mode must remove treble");
    require(filtered(2,8000)>filtered(2,200)*100,"HP mode must remove bass");
    require(filtered(1,1000)>filtered(1,50)*20,"BP mode must emphasize its band");
    SampleAudio impulse;impulse.rate=48000;for(auto& c:impulse.channel){c.assign(96,0);c[0]=1;}
    Rhythm rhythm;rhythm.enabled=true;rhythm.prepare(48000);for(auto& s:rhythm.samples)s=&impulse;
    for(int block=0;block<750;++block){rhythm.beginBlock(false,0,false);for(int i=0;i<512;++i){auto a=rhythm.next();require(std::isfinite(a[0]),"Drum output must remain finite");}}
    require(rhythm.hits[0]==8&&rhythm.hits[1]==8&&rhythm.hits[2]==32,"Standalone rhythm must stay on the 120 BPM grid over four bars");
    rhythm.reset();rhythm.beginBlock(false,0,false,true);for(int i=0;i<512;++i)rhythm.next();require(rhythm.hits[0]==0 && !rhythm.running(),"A host without PPQ must still obey Stop");
    rhythm.beginBlock(false,0,true,true);rhythm.next();require(rhythm.hits[0]==1 && rhythm.running(),"A host without PPQ must start the armed rhythm on Play");
    rhythm.reset();rhythm.custom={0,2,0,0,0};rhythm.pattern=6;rhythm.swing=.4f;rhythm.beginBlock(true,.27,true);int hit=-1;
    for(int i=0;i<3000;++i){rhythm.next();if(rhythm.hits[1]>0&&hit<0)hit=i;}
    require(std::abs(hit-1920)<=1,"Host sync must include a delayed swung step when starting between straight-grid positions");
    const auto oldHits=rhythm.hits;rhythm.beginBlock(true,.4,false);for(int i=0;i<1000;++i)rhythm.next();require(rhythm.hits==oldHits,"Stopped host transport must not trigger drums");
    rhythm.swing=0;rhythm.pattern=0;rhythm.beginBlock(true,0,true);rhythm.next();require(rhythm.hits[0]==oldHits[0]+1,"Host rewind must retrigger the correct downbeat");
    SampleAudio loop;loop.rate=48000;for(auto& c:loop.channel){c.resize(48000);for(size_t i=0;i<c.size();++i)c[i]=.1f*static_cast<float>(std::sin(tau*220*i/48000));}
    SampleVoice v;v.start(&loop,48000,2,1,true);for(int i=0;i<120000;++i){auto a=v.next(48000);require(std::isfinite(a[0]),"Looped samples must stay in bounds across pitch shifts");}v.stop();for(int i=0;i<3000;++i)v.next(48000);require(!v.audio,"Sample note-off must finish its release");
    SampleChords chords;chords.sound=&loop;chords.rootNote=60;chords.loop=true;chords.prepare(48000,16);chords.chord(0,3,true,0);
    double chordEnergy=0;for(int i=0;i<96000;++i){const auto a=chords.next();require(std::isfinite(a[0]),"Sampled chords must stay in bounds while looping");chordEnergy+=a[0]*a[0];}
    require(chordEnergy>1,"A single sample must create an audible polyphonic chord");chords.chord(0,3,false,0);for(int i=0;i<24000;++i)chords.next();require(chords.next()[0]==0,"Sampled chords must release all voices");
    SampleAudio beat;beat.rate=48000;beat.beats=4;
    for(auto& c:beat.channel){c.resize(115200);for(size_t i=0;i<c.size();++i)c[i]=.1f*static_cast<float>(std::sin(tau*440*i/beat.rate));}
    for(double tempo:{60.,100.,140.,200.}) {
        BeatLoop player;player.prepare(48000);double si=0,co=0,shiftedSin=0,shiftedCos=0;
        for(int i=0;i<96000;++i) {
            const double step=tempo/(60*48000);const auto a=player.next(&beat,i*step,step);
            require(std::isfinite(a[0]) && a[0]==a[1],"Time-stretched loops must stay finite and preserve stereo coherence");
            if(i>24000){si+=a[0]*std::sin(tau*440*i/48000);co+=a[0]*std::cos(tau*440*i/48000);
                shiftedSin+=a[0]*std::sin(tau*440*tempo/100*i/48000);shiftedCos+=a[0]*std::cos(tau*440*tempo/100*i/48000);}
        }
        require(std::hypot(si,co)>3000,"Tempo changes must preserve the recorded loop's pitch");
        if(tempo!=100)require(std::hypot(si,co)>100*std::hypot(shiftedSin,shiftedCos),"Loop synchronization must not use pitch-changing varispeed");
    }
    for(double rate:{8000.,16000.,44100.,48000.,96000.,192000.}) {
        Rhythm transport;transport.prepare(rate);transport.enabled=true;transport.useLoop=true;transport.loopSound=&beat;transport.bpm=140;
        transport.beginBlock(true,15.99,true);double energy=0;
        for(int i=0;i<4096;++i){const auto a=transport.next();require(std::isfinite(a[0]),"Loops must wrap safely across four-bar boundaries");energy+=a[0]*a[0];}
        require(energy>.1,"Recorded rhythm loops must produce audio");
        transport.beginBlock(true,0,false);for(int i=0;i<static_cast<int>(rate*.05);++i)transport.next();require(transport.next()[0]==0,"Host stop must fade recorded loops to silence");
        transport.beginBlock(true,0,true);for(int i=0;i<1024;++i)transport.next();require(std::abs(transport.next()[0])>1e-6,"Host rewind/play must restart recorded loops");
        transport.enabled=false;transport.reset();require(transport.next()[0]==0,"Loop panic must clear overlap and output history");
    }
    std::cout<<"FM operators, all 32 algorithms, unison 1-7, filters, samples and tempo-synced rhythm checks passed\n";
}
