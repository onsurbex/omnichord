#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "KeyboardMapping.h"
#include "Themes.h"

namespace {
void removeLegacyLogoPickers(juce::ValueTree node) {
    for(int i=node.getNumChildren()-1;i>=0;--i) {
        auto child=node.getChild(i);
        if(child.hasType("LogoPicker") || child.getProperty("id").toString()=="logo-picker") node.removeChild(i,nullptr);
        else removeLegacyLogoPickers(child);
    }
}
juce::ValueTree findNode(juce::ValueTree node,const juce::String& id) {
    if(node.getProperty("id").toString()==id) return node;
    for(auto child:node) { auto found=findNode(child,id); if(found.isValid()) return found; }
    return {};
}
void migrateButtonTypes(juce::ValueTree node) {
    for(int i=0;i<node.getNumChildren();++i) {
        auto child=node.getChild(i);
        const bool chord=child.hasType("TextButton") && child.getProperty("parameter").toString()=="chord";
        if(chord || child.hasType("ToggleButton")) {
            juce::ValueTree replacement(chord?"OmniChord":"OmniToggle");
            replacement.copyPropertiesAndChildrenFrom(child,nullptr);
            node.removeChild(i,nullptr); node.addChild(replacement,i,nullptr); child=replacement;
        }
        migrateButtonTypes(child);
    }
}
juce::ValueTree upgradeLayout(juce::ValueTree layout) {
    removeLegacyLogoPickers(layout);
    auto root=layout.getChildWithName("View");
    auto defaults=juce::ValueTree::fromXml(juce::String(BinaryData::magic_xml,static_cast<size_t>(BinaryData::magic_xmlSize)));
    auto shell=findNode(root,"shell");
    if(!shell.isValid()) return defaults;
    if(!findNode(shell,"sound-controls").isValid())
        shell.appendChild(findNode(defaults,"sound-controls").createCopy(),nullptr);
    if(!findNode(shell,"chord-36").isValid()) {
        for(int r=0;r<12;++r) {
            auto original=findNode(shell,"chord-"+juce::String(24+r));
            auto chord=original.isValid()?original.createCopy():findNode(defaults,"chord-"+juce::String(36+r)).createCopy();
            chord.setProperty("id","chord-"+juce::String(36+r),nullptr);
            chord.setProperty("button-radio-value",36+r,nullptr);
            const char* names[]{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
            chord.setProperty("text",juce::String(names[r])+"M7",nullptr);
            chord.setProperty("tooltip",juce::String(names[r])+"Maj7",nullptr);
            if(original.isValid()) chord.setProperty("pos-y",static_cast<int>(original.getProperty("pos-y"))+45,nullptr);
            shell.appendChild(chord,nullptr);
        }
        shell.appendChild(findNode(defaults,"quality-3").createCopy(),nullptr);
        for(const auto* id:{"sound-controls","divider-439","silence","footer"}) {
            auto n=findNode(shell,id);
            if(n.isValid()) n.setProperty("pos-y",static_cast<int>(n.getProperty("pos-y"))+45,nullptr);
        }
        root.setProperty("height",static_cast<int>(root.getProperty("height",650))+45,nullptr);
        shell.setProperty("pos-height",static_cast<int>(shell.getProperty("pos-height",620))+45,nullptr);
    }
    for(const auto* id:{"organSpread","organSpread-label","organChorus","organChorus-label"})
        if(!findNode(shell,id).isValid()) findNode(shell,"organ-controls").appendChild(findNode(defaults,id).createCopy(),nullptr);
    if(!findNode(shell,"theme").isValid()) shell.appendChild(findNode(defaults,"theme").createCopy(),nullptr);
    auto types=layout.getChildWithName("Styles").getChild(0).getChildWithName("Types");
    for(const auto* name:{"OmniChord","OmniToggle","ThemeSelector","ComboBox"})
        if(!types.getChildWithName(name).isValid()) types.appendChild(defaults.getChildWithName("Styles").getChild(0).getChildWithName("Types").getChildWithName(name).createCopy(),nullptr);
    migrateButtonTypes(root);
    root.setProperty("layout-version",3,nullptr);
    omnichord::prepareThemes(layout);
    return layout;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordHarpProcessor::parameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1},"Volume",0.0f,1.0f,0.65f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"tone",1},"Tone",0.0f,1.0f,0.35f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain",1},"Sustain",juce::NormalisableRange<float>(0.15f,5.0f,0.01f,0.5f),1.6f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"vibrato",1},"Vibrato",0.0f,1.0f,0.12f));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"chord",1},"Chord",0,35,0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"pad",1},"Chord organ",false));
    const auto addFloat=[&](const char* id,const char* name,float low,float high,float value) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,low,high,value));
    };
    addFloat("harpGain","Harp Volume",0,1,1);
    addFloat("organGain","Chord Volume",0,1,1);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"organSound",1},"Organ Sound",juce::StringArray{"Clasico","Reed","Suave","Brillante"},0));
    addFloat("organTone","Organ Tone",0,1,0.4f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"organOctave",1},"Organ Octave",juce::StringArray{"-1","0","+1"},1));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"harpSound",1},"Harp Sound",juce::StringArray{"Clasico","Campana","Seno","Cuerdas"},0));
    addFloat("harpAttack","Harp Attack (ms)",1,80,2);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"harpQuantize",1},"Harp Chord Notes",true));
    addFloat("delayTime","Delay Time (ms)",40,1200,350);
    addFloat("delayFeedback","Delay Feedback",0,0.85f,0.3f);
    addFloat("delayMix","Delay Mix",0,1,0);
    addFloat("reverbSize","Reverb Size",0,1,0.5f);
    addFloat("reverbDamping","Reverb Damping",0,1,0.5f);
    addFloat("reverbMix","Reverb Mix",0,1,0);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"computerKeys",1},"Mac Keyboard",true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"major7",1},"Major Seventh",false));
    addFloat("organSpread","Organ Spread",0,1,0.55f);
    addFloat("organChorus","Organ Chorus",0,1,0.35f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"theme",1},"Colour Theme",omnichord::themeNames(),0));
    return p;
}
ChordHarpProcessor::ChordHarpProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"ChordHarp",parameters()) {
    const char* ids[]{"gain","tone","sustain","vibrato","chord","pad","harpGain","organGain",
        "organSound","organTone","organOctave","harpSound","harpAttack","harpQuantize",
        "delayTime","delayFeedback","delayMix","reverbSize","reverbDamping","reverbMix","computerKeys","major7","organSpread","organChorus","theme"};
    for(size_t i=0;i<raw.size();++i) raw[i]=state.getRawParameterValue(ids[i]);
    chordParameter=state.getParameter("chord");
    major7Parameter=state.getParameter("major7");
    magicState.updateParameterMap();
    magicState.setGuiValueTree(upgradeLayout(juce::ValueTree::fromXml(juce::String(BinaryData::magic_xml,static_cast<size_t>(BinaryData::magic_xmlSize)))));
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    // Gives Save XML a useful starting directory for a local development build.
    const auto uiFile=juce::File(__FILE__).getParentDirectory().getSiblingFile("Assets").getChildFile("magic.xml");
    magicState.setResourcesFolder(uiFile.getFullPathName());
#endif
    magicState.addTrigger("silence",[this] {
        auto* parameter=state.getParameter("pad");
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();
        panic.store(true);
    });
}
void ChordHarpProcessor::prepareToPlay(double rate,int blockSize) {
    engine.prepare(rate);
    delay.milliseconds=raw[DelayTime]->load();
    delay.prepare(rate);
    reverb.setSampleRate(rate); reverb.reset();
    masterGain.reset(rate,0.02); masterGain.setCurrentAndTargetValue(raw[Gain]->load());
    organSilenced=false; lastChord=selectedChord(); lastPad=raw[Pad]->load()>0.5f;
    magicState.prepareToPlay(rate,blockSize);
}
bool ChordHarpProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet().isDisabled() && (b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() || b.getMainOutputChannelSet()==juce::AudioChannelSet::mono());
}
void ChordHarpProcessor::strum(int index) {
    const int chord=selectedChord();
    int a,n,b,m; fifo.prepareToWrite(1,a,n,b,m);
    if(n>0) notes[static_cast<size_t>(a)]=chordharp::chordNote(chord%12,chord/12,juce::jlimit(0,11,index));
    fifo.finishedWrite(n);
}
void ChordHarpProcessor::setChordFromMidi(int value) {
    value=juce::jlimit(0,47,value);
    major7Parameter->setValueNotifyingHost(value>=36?1.0f:0.0f);
    chordParameter->setValueNotifyingHost(chordParameter->convertTo0to1(static_cast<float>(value>=36?value%12:value)));
}
void ChordHarpProcessor::selectChord(int value) {
    chordParameter->beginChangeGesture(); major7Parameter->beginChangeGesture();
    setChordFromMidi(value);
    major7Parameter->endChangeGesture(); chordParameter->endChangeGesture();
    chordRetrigger.store(true);
}
int ChordHarpProcessor::selectedChord() const {
    const int chord=static_cast<int>(raw[Chord]->load());
    return raw[Major7]->load()>0.5f?36+chord%12:chord;
}
bool ChordHarpProcessor::computerKeyboardEnabled() const { return raw[ComputerKeys]->load()>0.5f; }
void ChordHarpProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    magicState.processMidiBuffer(midi,buffer.getNumSamples());
    const auto value=[this](Param p) { return raw[p]->load(); };
    engine.gain=1;
    engine.tone=value(Tone); engine.sustain=value(Sustain); engine.vibrato=value(Vibrato);
    engine.harpGain=value(HarpGain); engine.organGain=value(OrganGain);
    engine.organSound=static_cast<int>(value(OrganSound)); engine.organTone=value(OrganTone);
    engine.organOctave=static_cast<int>(value(OrganOctave))-1;
    engine.organSpread=value(OrganSpread); engine.organChorus=value(OrganChorus);
    engine.harpSound=static_cast<int>(value(HarpSound)); engine.attackSeconds=value(HarpAttack)*0.001f;
    masterGain.setTargetValue(value(Gain));
    delay.milliseconds=value(DelayTime); delay.feedback=value(DelayFeedback); delay.mix=value(DelayMix);
    juce::Reverb::Parameters ambience;
    ambience.roomSize=value(ReverbSize); ambience.damping=value(ReverbDamping);
    ambience.wetLevel=value(ReverbMix); ambience.dryLevel=1-value(ReverbMix); ambience.width=1;
    reverb.setParameters(ambience);
    const auto silence=[this] { engine.reset(); delay.reset(); reverb.reset(); organSilenced=true; };
    int activeChord=selectedChord();
    const bool pad=value(Pad)>0.5f;
    if(chordRetrigger.exchange(false) || activeChord!=lastChord || pad!=lastPad) organSilenced=false;
    const bool stop=panic.exchange(false);
    if(stop) silence();
    engine.chord(activeChord%12,activeChord/12,pad && !organSilenced);
    int a,n,b,m; fifo.prepareToRead(fifo.getNumReady(),a,n,b,m);
    if(!stop) {
        for(int i=0;i<n;++i) engine.pluck(notes[static_cast<size_t>(a+i)]);
        for(int i=0;i<m;++i) engine.pluck(notes[static_cast<size_t>(b+i)]);
    }
    fifo.finishedRead(n+m);
    auto event=midi.cbegin();
    for(int sample=0;sample<buffer.getNumSamples();++sample) {
        auto groupEnd=event;
        while(groupEnd!=midi.cend() && (*groupEnd).samplePosition<=sample) ++groupEnd;
        // Chord selection precedes harp notes sharing a MIDI timestamp.
        for(auto it=event;it!=groupEnd;++it) {
            const auto message=(*it).getMessage();
            if(message.isNoteOn()) {
                const int chord=chordharp::chordFromMidi(message.getNoteNumber());
                if(chord>=0) {
                    activeChord=chord; organSilenced=false;
                    setChordFromMidi(chord);
                    engine.chord(chord%12,chord/12,pad);
                }
            }
        }
        for(auto it=event;it!=groupEnd;++it) {
            const auto message=(*it).getMessage();
            if(message.isNoteOn() && message.getNoteNumber()>=72)
                engine.pluck(chordharp::harpFromMidi(message.getNoteNumber(),activeChord,value(HarpQuantize)>0.5f),message.getFloatVelocity());
            if(message.isAllSoundOff() || message.isAllNotesOff()) silence();
        }
        event=groupEnd;
        engine.next();
        float left=engine.leftOutput,right=engine.rightOutput;
        delay.process(left,right);
        reverb.processStereo(&left,&right,1);
        const float master=masterGain.getNextValue();
        left=std::tanh(left*master); right=std::tanh(right*master);
        if(buffer.getNumChannels()==1) buffer.setSample(0,sample,(left+right)*0.5f);
        else { buffer.setSample(0,sample,left); buffer.setSample(1,sample,right); }
    }
    lastChord=activeChord; lastPad=pad;
    midi.clear();
}
void ChordHarpProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=state.copyState();
    snapshot.removeProperty("customLogo",nullptr);
    snapshot.setProperty("guiLayout",magicState.getGuiTree().toXmlString(),nullptr);
    if(auto xml=snapshot.createXml()) copyXmlToBinary(*xml,dest);
}
void ChordHarpProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto restored=juce::ValueTree::fromXml(*xml);
        restored.removeProperty("customLogo",nullptr);
        const auto layout=juce::ValueTree::fromXml(restored.getProperty("guiLayout").toString());
        if(layout.hasType("magic") && layout.getChildWithName("View").isValid())
            magicState.setGuiValueTree(upgradeLayout(layout));
        for(const auto* id:{"major7","organSpread","organChorus","theme"}) {
            auto* parameter=state.getParameter(id);
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
        state.replaceState(restored);
    }
}
juce::AudioProcessorEditor* ChordHarpProcessor::createEditor() { return new ChordHarpEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordHarpProcessor; }
