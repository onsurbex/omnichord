#include "MixerDSP.h"
#include "ChordSynth.h"
#include "SampleChords.h"
#include "KeyboardMapping.h"
#include "Tape.h"
#include "SynthPatches.h"
#include <cstdlib>
#include <iostream>
static void require(bool c,const char* text){if(!c){std::cerr<<text<<'\n';std::exit(1);}}
int main(){
    using namespace chordharp;
    const auto left=stereoPan({0,1},-1.f);require(left[0]>.5f && left[1]==0,"Hard pan must fold the right input into the left speaker");
    const auto mono=stereoWidth({.8f,.2f},0);require(mono[0]==mono[1],"Width zero must produce mono");
    const auto wide=stereoWidth({.8f,.2f},2);require(std::abs(wide[0]-wide[1])>1,"Width two must widen the side component");
    MixerDSP mixer;for(auto& c:mixer.channels)c.enabled=false;mixer.channels[0]={true,false,.5f,0,.3f,.4f};mixer.prepare(48000);
    const std::array<Stereo,5> source{{{1,1},{},{},{},{}}};auto feed=mixer.feed(source);
    require(std::abs(feed.dry[0]-.5f)<1e-6 && std::abs(feed.delay[0]-.15f)<1e-6 && std::abs(feed.reverb[0]-.2f)<1e-6,"Sends must use the channel's post-fader level");
    mixer.channels[1]={true,true,1,0,0,0};for(int n=0;n<16000;++n)feed=mixer.feed(source);require(std::abs(feed.dry[0])<1e-6,"Soloing Organ must remove Harp from the dry mix and its sends");
    mixer.channels[1].solo=false;mixer.returns[0].solo=true;for(int n=0;n<16000;++n)feed=mixer.feed(source);
    require(std::abs(feed.dry[0])<1e-6 && feed.delay[0]>.14f,"Soloing an FX return must keep its send feed while muting dry sources");
    mixer.width=0;Stereo sum{};for(int n=0;n<16000;++n)sum=mixer.output(mixer.sumReturns({.2f,.8f},{{{}, {}}}));require(std::abs(sum[0]-sum[1])<1e-6,"Master Width must fold the complete mix to mono");
    mixer.masterEnabled=false;for(int n=0;n<1200;++n)mixer.output({.2f,-.2f});require(mixer.output({.8f,.9f})==Stereo{},"Master mute must silence audio after the Tape insert");
    require(chromaticFromComputerKey(U'A',3)==60 && chromaticFromComputerKey(U'W',3)==61 && chromaticFromComputerKey(U'S',3)==62 && chromaticFromComputerKey(U'1',3)==72,"Mac chromatic keys must play semitones and numbers the upper octave");
    for(double rate:{44100.,48000.,96000.}) {
        for(int model=0;model<10;++model) {
            ChordSynth organ;organ.model=model;organ.unison.voices=3;organ.prepare(rate);organ.noteOn(60,9,.8f);organ.noteOn(64,10,.8f);double power=0;
            for(int n=0;n<static_cast<int>(rate*.15);++n){const auto a=organ.next();require(std::isfinite(a[0]) && std::abs(a[0])<1,"All organ registrations must remain bounded");power+=a[0]*a[0];}
            require(power>.001,"Each subtractive organ registration must produce sound");organ.noteOff(9);
            double remaining=0;for(int n=0;n<static_cast<int>(rate*.1);++n)remaining+=std::abs(organ.next()[0]);require(remaining>.01,"Releasing one chromatic note must keep the other note alive");
            organ.noteOff(10);for(int n=0;n<static_cast<int>(rate*2);++n)organ.next();require(std::abs(organ.next()[0])<1e-5,"Chromatic organ note-off must finish its release");
        }
        ChordSynth fm;fm.fm=true;fm.patch=synthPatch(0,true);for(auto& op:fm.patch.op)op.release=.02f;fm.prepare(rate);fm.noteOn(60,1,.8f);fm.noteOn(67,2,.8f);
        double power=0;for(int n=0;n<static_cast<int>(rate*.15);++n)power+=std::abs(fm.next()[0]);require(power>.01,"Polyphonic FM must be audible independently from Organ");
        fm.noteOff(1);fm.noteOff(2);for(int n=0;n<static_cast<int>(rate*.5);++n)fm.next();require(std::abs(fm.next()[0])<1e-5,"FM notes must release independently");
    }
    ChordSynth plain,detuned;for(auto* s:{&plain,&detuned}){s->unison.voices=7;s->unison.drift=0;s->vibrato=0;s->unison.detune=0;}
    detuned.unison.detune=35;plain.prepare(48000);detuned.prepare(48000);plain.chord(0,0,true);detuned.chord(0,0,true);double difference=0;
    for(int n=0;n<24000;++n){auto a=plain.next(),b=detuned.next();difference+=std::abs(a[0]-b[0]);}require(difference>10,"Detune must change the rendered audio when seven voices are active");
    Unison u;u.voices=7;u.drift=0;u.detune=35;u.prepare(48000);require(u.ratio[5]<.99f && u.ratio[6]>1.01f,"Outer unison voices must receive real opposite tuning offsets");
    SampleAudio sound;sound.rate=48000;for(auto& c:sound.channel){c.resize(48000);for(size_t i=0;i<c.size();++i)c[i]=.1f*std::sin(tau*220*i/48000);}
    SampleChords samples;samples.sound=&sound;samples.loop=true;samples.release=.02f;samples.prepare(48000,16);samples.noteOn(60,73,.8f,0);samples.chord(2,0,false,0);
    double samplePower=0;for(int n=0;n<24000;++n)samplePower+=std::abs(samples.next()[0]);require(samplePower>.01,"Changing the memorized chord must not stop chromatic sample notes");
    samples.noteOff(73);for(int n=0;n<4800;++n)samples.next();require(samples.next()==Stereo{},"Chromatic samples must release on note-off");
    std::cout<<"Mixer sends, pan, width, solos, master mute, organ/FM/sample polyphony and audible detune passed\n";
}
