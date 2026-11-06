#include "PluginProcessor.h"
#include <cstdlib>
#include <cmath>

juce::File ChordHarpProcessor::defaultPresetHome() {
    auto base=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    base=base.getChildFile("Application Support");
#endif
    return base.getChildFile("BLINBLIN/Omnichord");
}
juce::ValueTree ChordHarpProcessor::capturePreset(const juce::String& name) const {
    juce::ValueTree preset("OmnichordPreset",{{"formatVersion",1},{"name",name}});
    for(auto* parameter:getParameters()) if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
        preset.appendChild(juce::ValueTree("PARAM",{{"id",ranged->paramID},{"value",ranged->convertFrom0to1(ranged->getValue())}}),nullptr);
    }
    return preset;
}
bool ChordHarpProcessor::applyPreset(const juce::ValueTree& preset,juce::String& error) {
    if(!preset.hasType("OmnichordPreset") || static_cast<int>(preset.getProperty("formatVersion"))!=1) {
        error="Formato de preset no compatible."; return false;
    }
    juce::StringArray seen; int known=0;
    for(auto child:preset) {
        const auto id=child.getProperty("id").toString(); auto* parameter=state.getParameter(id);
        if(!parameter) continue;
        const auto text=child.getProperty("value").toString().trim();
        const auto bytes=text.toRawUTF8(); char* end=nullptr; const double value=std::strtod(bytes,&end);
        const auto& range=parameter->getNormalisableRange();
        if(seen.contains(id) || text.isEmpty() || end==bytes || *end!=0 || !std::isfinite(value)
            || value<range.start-1e-5 || value>range.end+1e-5) {
            error="Valor invalido en el preset: "+id; return false;
        }
        seen.add(id); ++known;
    }
    if(known==0) { error="El preset no contiene parametros reconocidos."; return false; }
    // Validate the complete document before changing any parameter.
    for(auto* parameter:getParameters()) if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
        auto value=preset.getChildWithProperty("id",ranged->paramID);
        const auto normalized=value.isValid()?ranged->convertTo0to1(juce::jlimit(ranged->getNormalisableRange().start,ranged->getNormalisableRange().end,static_cast<float>(value.getProperty("value")))):ranged->getDefaultValue();
        ranged->beginChangeGesture(); ranged->setValueNotifyingHost(normalized); ranged->endChangeGesture();
    }
    updatePresetName(preset.getProperty("name","Preset").toString());
    panic.store(true); error.clear(); return true;
}
bool ChordHarpProcessor::writePreset(const juce::File& file,const juce::ValueTree& data,juce::String& error) const {
    if(file==juce::File{} || !file.getParentDirectory().createDirectory().wasOk()) { error="No se puede crear la carpeta del preset."; return false; }
    juce::TemporaryFile temporary(file);
    if(!temporary.getFile().replaceWithText(data.toXmlString()) || !temporary.overwriteTargetFileWithTemporary()) {
        error="No se ha podido guardar el preset."; return false;
    }
    error.clear(); return true;
}
bool ChordHarpProcessor::savePreset(const juce::File& file,juce::String& error) {
    const auto name=file.getFileNameWithoutExtension();
    if(!writePreset(file,capturePreset(name),error)) return false;
    updatePresetName(name,"file:"+file.getFullPathName()); return true;
}
bool ChordHarpProcessor::loadPreset(const juce::File& file,juce::String& error) {
    if(!file.existsAsFile() || file.getSize()>1024*1024) { error="Archivo de preset no valido."; return false; }
    if(!applyPreset(juce::ValueTree::fromXml(file.loadFileAsString()),error)) return false;
    updatePresetName(getPresetName(),"file:"+file.getFullPathName()); return true;
}
bool ChordHarpProcessor::setStartupPreset(juce::String& error) {
    return writePreset(presetHome.getChildFile("Startup.omnipreset"),capturePreset(getPresetName()),error);
}
bool ChordHarpProcessor::clearStartupPreset(juce::String& error) {
    const auto file=presetHome.getChildFile("Startup.omnipreset");
    if(file.existsAsFile() && !file.deleteFile()) { error="No se ha podido restablecer el preset de inicio."; return false; }
    error.clear(); return true;
}
void ChordHarpProcessor::loadStartupPreset() {
    const auto file=presetHome.getChildFile("Startup.omnipreset");
    if(file.existsAsFile()) { juce::String error; loadPreset(file,error); }
    panic.store(false);
}
juce::StringArray ChordHarpProcessor::factoryPresetNames() {
    return {"Inicio","Iglesia Principal","Catedral ambiental","Tonewheel Jazz","Tonewheel Gospel","Tonewheel Rock","Omnichord clasico"};
}
bool ChordHarpProcessor::loadFactoryPreset(int index,juce::String& error) {
    const auto names=factoryPresetNames();
    if(index<0 || index>=names.size()) { error="Preset no valido."; return false; }
    auto data=factoryPreset.createCopy(); data.setProperty("name",names[index],nullptr);
    const auto set=[&](const char* id,float value) { data.getChildWithProperty("id",id).setProperty("value",value,nullptr); };
    if(index>0) {
        constexpr int models[]{4,4,6,8,9,10,0};
        set("organModel",static_cast<float>(models[index])); set("pad",1); set("chordSustain",0);
        set("organTone",index==2?0.7f:0.6f); set("reverbMix",index==2?0.42f:index<3?0.2f:0.1f);
        set("reverbSize",index==2?0.85f:0.5f);
    }
    if(!applyPreset(data,error)) return false;
    updatePresetName(names[index],"factory:"+juce::String(index)); return true;
}

void ChordHarpProcessor::updatePresetName(const juce::String& name,const juce::String& source) { const juce::ScopedLock lock(presetLock); presetName=name; presetSource=source; ++presetRevision; }
