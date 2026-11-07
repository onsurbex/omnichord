#include "PluginProcessor.h"

namespace {
constexpr const char* fields[]{"Ratio","Fine","Level","Attack","Decay","Sustain","Release","Velocity"};
std::array<float,8> values(const chordharp::FMOperator& op) {return {op.ratio,op.fine,op.level,op.attack*1000,op.decay*1000,op.sustain,op.release*1000,op.velocity};}
}
juce::String ChordHarpProcessor::fmOperatorID(bool organ,int op,const juce::String& field) {return juce::String(organ?"organ":"harp")+"Op"+juce::String(op+1)+field;}
juce::StringArray ChordHarpProcessor::synthSoundNames(bool organ) {
    juce::StringArray names=organ?juce::StringArray{"Clasico","Reed","Suave"}:juce::StringArray{"Clasico"};
    for(const auto* name:chordharp::fmPatchNames) names.add(name);if(organ)names.add("Samples");return names;
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
    engine.tone=raw[Tone]->load();engine.sustain=raw[Sustain]->load();engine.vibrato=raw[Vibrato]->load();
    engine.organTone=raw[OrganTone]->load();engine.organOctave=static_cast<int>(raw[OrganOctave]->load())-1;
    engine.harpGain=raw[HarpGain]->load();engine.organGain=raw[OrganGain]->load();engine.attackSeconds=raw[HarpAttack]->load()*.001f;
    for(size_t bank=0;bank<2;++bank) {
        if(synthUpdating[bank].load())continue;
        const bool organ=bank==1;const auto& r=synthRefs[bank].common;auto& u=organ?engine.organUnison:engine.harpUnison;
        u.voices=static_cast<int>(r[0]->load());u.detune=r[1]->load();u.width=r[2]->load();u.drift=r[3]->load();
        auto& filter=organ?engine.organFilter:engine.harpFilter;filter.cutoff=r[4]->load();filter.resonance=r[5]->load();filter.mode=static_cast<int>(r[6]->load());
        (organ?engine.organPatch:engine.harpPatch)=getFMPatch(organ);
        if(organ)engine.organModel=static_cast<int>(raw[OrganModel]->load());else engine.harpSound=static_cast<int>(raw[HarpSound]->load());
    }
    engine.configure();
}
int ChordHarpProcessor::getSynthChoice(bool organ) const {
    const int model=static_cast<int>((organ?raw[OrganModel]:raw[HarpSound])->load()),offset=organ?3:1;
    if(organ && model==4)return offset+static_cast<int>(chordharp::fmPatchNames.size());
    if(model<offset)return model;
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
    const int offset=organ?3:1;if(choice<0 || choice>=offset+static_cast<int>(chordharp::fmPatchNames.size())+(organ?1:0))return;
    const auto set=[&](const juce::String& id,float value) {if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}};
    synthUpdating[organ?1u:0u].store(true);
    if(organ && choice==11) {set("organModel",4);synthUpdating[1].store(false);return;}
    if(choice>=offset) {
        const auto patch=chordharp::synthPatch(choice-offset,organ);const juce::String prefix=organ?"organ":"harp";
        set(prefix+"Algorithm",static_cast<float>(patch.algorithm+1));set(prefix+"Feedback",static_cast<float>(patch.feedback));
        for(int op=0;op<6;++op) {const auto v=values(patch.op[static_cast<size_t>(op)]);for(size_t f=0;f<8;++f)set(fmOperatorID(organ,op,fields[f]),v[f]);}
        set(prefix+"Cutoff",12000);
    }
    set(organ?"organModel":"harpSound",static_cast<float>(choice<offset?choice:offset));
    synthUpdating[organ?1u:0u].store(false);
}
