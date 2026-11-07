#include "PluginProcessor.h"

juce::ValueTree ChordHarpProcessor::migrateV6(juce::ValueTree source) {
    auto out=source.createCopy();
    const auto get=[&](const juce::String& id,float fallback=0.f){auto p=source.getChildWithProperty("id",id);return p.isValid()?static_cast<float>(p.getProperty("value")):fallback;};
    const auto set=[&](const juce::String& id,float value){auto p=out.getChildWithProperty("id",id);if(p.isValid())p.setProperty("value",value,nullptr);else out.appendChild(juce::ValueTree("PARAM",{{"id",id},{"value",value}}),nullptr);};
    const float model=get("organModel");const int oldModel=std::isfinite(model)?static_cast<int>(std::clamp(model,0.f,4.f)):0;
    set("harpEnabled",1);set("organEnabled",oldModel<3?1.f:0.f);set("fmEnabled",oldModel==3?1.f:0.f);set("samplesEnabled",oldModel==4?1.f:0.f);
    set("organModel",oldModel<3?static_cast<float>(oldModel):0.f);
    set("fmGain",get("organGain",1));set("sampleGain",get("organGain",1));set("fmTone",get("organTone",.4f));
    set("fmOctave",get("organOctave",1));set("sampleOctave",get("organOctave",1));
    for(const auto* field:{"Unison","Detune","Width","Drift","Cutoff","Resonance","FilterType"}) {
        const auto old=source.getChildWithProperty("id","organ"+juce::String(field));
        if(old.isValid())set("fm"+juce::String(field),static_cast<float>(old.getProperty("value")));
    }
    for(const auto* field:{"Cutoff","Resonance","FilterType"})set("sample"+juce::String(field),get("organ"+juce::String(field),juce::String(field)=="Cutoff"?4000.f:juce::String(field)=="Resonance"?.707f:0.f));
    const float delay=get("delayMix"),reverb=get("reverbMix");
    for(int channel=0;channel<5;++channel) {
        const float input=channel==4?get("rhythmSend"):1.f;
        set(mixerParameter(channel,"DelaySend"),delay*input);set(mixerParameter(channel,"ReverbSend"),reverb*input);
    }
    set("delayMix",1);set("reverbMix",1);set("masterEnabled",1);set("masterWidth",1);set("fxEnabled",1);set("playMode",0);
    for(auto* p:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(p))
        if(!out.getChildWithProperty("id",parameter->paramID).isValid())set(parameter->paramID,parameter->convertFrom0to1(parameter->getDefaultValue()));
    if(out.hasType("OmnichordPreset"))out.setProperty("formatVersion",3,nullptr);else out.setProperty("synthVersion",7,nullptr);
    return out;
}
