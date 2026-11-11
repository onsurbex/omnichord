#include "PluginProcessor.h"

namespace {
constexpr const char* fields[]{"Ratio","Fine","Level","Attack","Decay","Sustain","Release","Velocity"};
std::array<float,8> values(const chordharp::FMOperator& op) {return {op.ratio,op.fine,op.level,op.attack*1000,op.decay*1000,op.sustain,op.release*1000,op.velocity};}
}
juce::String ChordHarpProcessor::fmOperatorID(bool organ,int op,const juce::String& field) {return juce::String(organ?"organ":"harp")+"Op"+juce::String(op+1)+field;}
juce::StringArray ChordHarpProcessor::synthSoundNames(bool organ) const {
    juce::StringArray names;if(!organ)names.add("Clasico");for(const auto* name:chordharp::fmPatchNames)names.add(name);if(!organ)names.add("Grayslate samples");return names;
}
chordharp::FMPatch ChordHarpProcessor::getFMPatch(bool organ) const {
    const auto& refs=synthRefs[organ?1u:0u];chordharp::FMPatch p;
    p.algorithm=static_cast<int>(refs.common[7]->load())-1;p.feedback=static_cast<int>(refs.common[8]->load());
    for(size_t i=0;i<6;++i) {
        auto& o=p.op[i];const auto& r=refs.op[i];o.ratio=r[0]->load();o.fine=r[1]->load();o.level=r[2]->load();
        o.attack=r[3]->load()*.001f;o.decay=r[4]->load()*.001f;o.sustain=r[5]->load();o.release=r[6]->load()*.001f;o.velocity=r[7]->load();
    }
    return p;
}
void ChordHarpProcessor::readSynthParameters() {
    engine.gain=1;engine.organGain=0;engine.harpGain=1;engine.chord(0,0,false);engine.chromatic=chromaticMode();
    engine.tone=raw[Tone]->load();engine.sustain=raw[Sustain]->load();engine.vibrato=raw[Vibrato]->load();engine.attackSeconds=raw[HarpAttack]->load()*.001f;
    for(size_t bank=0;bank<2;++bank) {
        if(synthUpdating[bank].load())continue;
        const bool fm=bank==1;const auto& r=synthRefs[bank].common;auto& u=fm?fmLayer.unison:engine.harpUnison;
        u.voices=static_cast<int>(r[0]->load());u.detune=r[1]->load();u.width=r[2]->load();u.drift=r[3]->load();
        auto& filter=fm?fmLayer.filter:engine.harpFilter;filter.cutoff=r[4]->load();filter.resonance=r[5]->load();filter.mode=static_cast<int>(r[6]->load());
        if(fm)fmLayer.patch=getFMPatch(true);else {engine.harpPatch=getFMPatch(false);engine.harpSound=static_cast<int>(raw[HarpSound]->load());}
    }
    auto& u=organLayer.unison;u.voices=static_cast<int>(organRefs[0]->load());u.detune=organRefs[1]->load();u.width=organRefs[2]->load();u.drift=organRefs[3]->load();
    organLayer.filter.cutoff=organRefs[4]->load();organLayer.filter.resonance=organRefs[5]->load();organLayer.filter.mode=static_cast<int>(organRefs[6]->load());
    organLayer.model=static_cast<int>(raw[OrganModel]->load());organLayer.colour=raw[OrganTone]->load();organLayer.octave=static_cast<int>(raw[OrganOctave]->load())-1;organLayer.vibrato=raw[Vibrato]->load();
    fmLayer.colour=mixerRaw[6]->load();fmLayer.octave=static_cast<int>(mixerRaw[7]->load())-1;fmLayer.vibrato=raw[Vibrato]->load();
    grayHarpPlayer.bank=grayHarpBank.get();grayChordPlayer.bank=chromaticMode()?grayHarpBank.get():grayChordBank.get();
    grayHarpPlayer.attack=raw[HarpAttack]->load()*.001f;
    grayHarpFilter.cutoff=synthRefs[0].common[4]->load();grayHarpFilter.resonance=synthRefs[0].common[5]->load();grayHarpFilter.mode=static_cast<int>(synthRefs[0].common[6]->load());
    grayChordPlayer.release=sampleChords.release;grayChordPlayer.loop=sampleChords.loop;
    engine.configure();organLayer.configure();fmLayer.configure();
}
int ChordHarpProcessor::getSynthChoice(bool organ) const {
    const int offset=organ?0:1;
    if(!organ && graySlateHarp())return 1+static_cast<int>(chordharp::fmPatchNames.size());
    if(!organ && raw[HarpSound]->load()<.5f)return 0;
    const auto current=getFMPatch(organ);
    for(int preset=0;preset<static_cast<int>(chordharp::fmPatchNames.size());++preset) {
        const auto candidate=chordharp::synthPatch(preset,organ);bool match=current.algorithm==candidate.algorithm && current.feedback==candidate.feedback;
        for(size_t op=0;op<6 && match;++op) {
            const auto a=values(current.op[op]),b=values(candidate.op[op]);
            for(size_t field=0;field<8;++field) if(std::abs(a[field]-b[field])>std::max(.001f,std::abs(b[field])*1e-5f))match=false;
        }
        if(match)return offset+preset;
    }
    return -1;
}
void ChordHarpProcessor::selectSynthSound(bool organ,int choice) {
    if(!organ && choice==1+static_cast<int>(chordharp::fmPatchNames.size())) {
        juce::String error;if(!ensureGraySlateBanks(false,true,error))return;
        auto* p=state.getParameter("harpGraySlate");p->beginChangeGesture();p->setValueNotifyingHost(1);p->endChangeGesture();return;
    }
    const int offset=organ?0:1;if(choice<0 || choice>=offset+static_cast<int>(chordharp::fmPatchNames.size()))return;
    const auto set=[&](const juce::String& id,float value) {if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}};
    if(!organ)set("harpGraySlate",0);
    synthUpdating[organ?1u:0u].store(true);
    if(choice>=offset) {
        const auto patch=chordharp::synthPatch(choice-offset,organ);const juce::String prefix=organ?"organ":"harp";
        set(prefix+"Algorithm",static_cast<float>(patch.algorithm+1));set(prefix+"Feedback",static_cast<float>(patch.feedback));
        for(int op=0;op<6;++op) {const auto v=values(patch.op[static_cast<size_t>(op)]);for(size_t f=0;f<8;++f)set(fmOperatorID(organ,op,fields[f]),v[f]);}
        set(juce::String(organ?"fm":"harp")+"Cutoff",12000);
    }
    if(organ)set("fmEnabled",1);else set("harpSound",static_cast<float>(choice<offset?choice:offset));
    synthUpdating[organ?1u:0u].store(false);
}
