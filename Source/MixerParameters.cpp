#include "PluginProcessor.h"

juce::String ChordHarpProcessor::mixerParameter(int channel,const juce::String& role) {
    if(channel<0 || channel>7)return {};
    constexpr const char* prefix[]{"harp","organ","fm","sample","rhythm","delay","reverb","master"};
    constexpr const char* gains[]{"harpGain","organGain","fmGain","sampleGain","rhythmVolume","delayMix","reverbMix","gain"};
    constexpr const char* enabled[]{"harpEnabled","organEnabled","fmEnabled","samplesEnabled","rhythmEnabled","delayEnabled","reverbEnabled","masterEnabled"};
    if(role=="Gain")return gains[channel];if(role=="Enabled")return enabled[channel];
    if(role=="Solo" && channel==7)return {};
    return juce::String(prefix[channel])+role;
}
bool ChordHarpProcessor::chromaticMode() const{return mixerRaw[4]->load()>.5f;}
int ChordHarpProcessor::computerOctave() const{return static_cast<int>(mixerRaw[5]->load());}
void ChordHarpProcessor::shiftComputerOctave(int amount) {
    auto* p=state.getParameter("computerOctave");p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(juce::jlimit(-2,6,computerOctave()+amount))));p->endChangeGesture();
}
void ChordHarpProcessor::computerNoteOn(int note,float velocity){computerKeys.noteOn(1,note,velocity);}
void ChordHarpProcessor::computerNoteOff(int note){computerKeys.noteOff(1,note,0);}
void ChordHarpProcessor::registerSoloCleanup() {
    for(int i=0;i<7;++i) {
        const auto index=static_cast<size_t>(i);mixerEnableIDs[index]=mixerParameter(i,"Enabled");
        enableParameters[index]=state.getParameter(mixerEnableIDs[index]);soloParameters[index]=state.getParameter(mixerParameter(i,"Solo"));
        state.addParameterListener(mixerEnableIDs[index],this);
    }
    state.addParameterListener("pad",this);state.addParameterListener("fxEnabled",this);
}
ChordHarpProcessor::~ChordHarpProcessor() {
    for(const auto& id:mixerEnableIDs)state.removeParameterListener(id,this);
    state.removeParameterListener("pad",this);state.removeParameterListener("fxEnabled",this);
}
void ChordHarpProcessor::clearMixerSolo(int channel) {
    auto* parameter=soloParameters[static_cast<size_t>(channel)];
    if(parameter && parameter->getValue()>.5f)parameter->setValueNotifyingHost(0);
}
void ChordHarpProcessor::parameterChanged(const juce::String& id,float value) {
    if(value>=.5f)return;
    if(id=="pad"){for(int i=1;i<4;++i)clearMixerSolo(i);return;}
    if(id=="fxEnabled"){clearMixerSolo(5);clearMixerSolo(6);return;}
    for(int i=0;i<7;++i)if(id==mixerEnableIDs[static_cast<size_t>(i)]){clearMixerSolo(i);return;}
}
void ChordHarpProcessor::clearDisabledMixerSolos() {
    for(int i=0;i<7;++i) {
        const bool groupOff=(i>=1 && i<=3 && raw[Pad]->load()<.5f) || (i>=5 && mixerRaw[3]->load()<.5f);
        if(groupOff || enableParameters[static_cast<size_t>(i)]->getValue()<.5f)clearMixerSolo(i);
    }
}
void ChordHarpProcessor::toggleMixerSolo(int channel) {
    if(channel<0 || channel>7)return;
    const auto set=[&](const juce::String& id,float value){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(value);p->endChangeGesture();}};
    if(channel==7){for(int i=0;i<7;++i)clearMixerSolo(i);return;}
    const bool on=soloParameters[static_cast<size_t>(channel)]->getValue()>.5f;
    if(!on) {
        if(channel>=1 && channel<=3)set("pad",1);
        if(channel>=5)set("fxEnabled",1);
        set(mixerEnableIDs[static_cast<size_t>(channel)],1);
    }
    set(mixerParameter(channel,"Solo"),on?0.f:1.f);
}
void ChordHarpProcessor::readMixerParameters() {
    // Fixed source calibration: Organ +9 dB, FM +7.5 dB; faders keep unity at 0 dB.
    constexpr std::array<float,5> calibration{1.f,2.818383f,2.371374f,1.f,1.f};
    for(size_t i=0;i<5;++i) {
        const auto& r=mixerSources[i].value;auto& c=mixer.channels[i];
        c.enabled=r[0]->load()>.5f;c.solo=r[1]->load()>.5f;c.gain=r[2]->load()*calibration[i];c.pan=r[3]->load();c.delay=r[4]->load();c.reverb=r[5]->load();
    }
    for(size_t i=0;i<2;++i) {
        const auto& r=mixerReturns[i];auto& c=mixer.returns[i];c.enabled=r[0]->load()>.5f;c.solo=r[1]->load()>.5f;c.gain=r[2]->load();c.width=r[3]->load();
    }
    mixer.clipperEnabled=masterClipperRaw->load()>.5f;
    mixer.master=mixerRaw[0]->load();mixer.masterEnabled=mixerRaw[1]->load()>.5f;mixer.width=mixerRaw[2]->load();mixer.fxEnabled=mixerRaw[3]->load()>.5f;
}
void ChordHarpProcessor::liveNoteOn(int note,int source,float velocity) {
    if(source<0 || source>=static_cast<int>(liveNotes.size()))return;
    auto& key=liveNotes[static_cast<size_t>(source)];key.velocity=velocity;key.down=true;key.sustained=false;
    if(mixer.channels[0].enabled)engine.pluck(note,velocity,source);
    if(raw[Pad]->load()>.5f) {
        if(mixer.channels[1].enabled)organLayer.noteOn(note,source,velocity);
        if(mixer.channels[2].enabled)fmLayer.noteOn(note,source,velocity);
        if(mixer.channels[3].enabled)sampleChords.noteOn(note,source,velocity,static_cast<int>(mixerRaw[8]->load())-1);
    }
    tapeMuted=false;
}
void ChordHarpProcessor::liveNoteOff(int source) {
    if(source<0 || source>=static_cast<int>(liveNotes.size()))return;
    auto& key=liveNotes[static_cast<size_t>(source)];key.down=false;
    if(pedals[static_cast<size_t>(source/128)]){key.sustained=true;return;}
    key.sustained=false;engine.releaseHarp(source);organLayer.noteOff(source);fmLayer.noteOff(source);sampleChords.noteOff(source);
}
void ChordHarpProcessor::releaseLiveNotes() {
    liveNotes={};pedals={};engine.releaseAllHarp();organLayer.releaseAll();fmLayer.releaseAll();sampleChords.releaseAll();
}
void ChordHarpProcessor::restoreHeldLayer(int layer) {
    for(size_t source=0;source<liveNotes.size();++source) {
        const auto& n=liveNotes[source];if(!n.down && !n.sustained)continue;
        const int note=static_cast<int>(source%128),id=static_cast<int>(source);
        if(layer==0)engine.pluck(note,n.velocity,id);
        else if(layer==1)organLayer.noteOn(note,id,n.velocity);
        else if(layer==2)fmLayer.noteOn(note,id,n.velocity);
        else sampleChords.noteOn(note,id,n.velocity,static_cast<int>(mixerRaw[8]->load())-1);
    }
}
