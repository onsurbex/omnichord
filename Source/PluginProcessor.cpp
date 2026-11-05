#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "KeyboardMapping.h"

namespace {
void removeLegacyLogoPickers(juce::ValueTree node) {
    for(int i=node.getNumChildren()-1;i>=0;--i) {
        auto child=node.getChild(i);
        if(child.hasType("LogoPicker") || child.getProperty("id").toString()=="logo-picker") node.removeChild(i,nullptr);
        else removeLegacyLogoPickers(child);
    }
}
juce::ValueTree upgradeLayout(juce::ValueTree layout) {
    removeLegacyLogoPickers(layout);
    auto root=layout.getChildWithName("View");
    if(static_cast<int>(root.getProperty("layout-version",0))>=2) return layout;
    const auto defaults=juce::ValueTree::fromXml(juce::String(BinaryData::magic_xml,static_cast<size_t>(BinaryData::magic_xmlSize)));
    const auto newRoot=defaults.getChildWithName("View");
    auto shell=root.getChildWithProperty("id","shell");
    const auto newShell=newRoot.getChildWithProperty("id","shell");
    if(!shell.isValid()) return defaults;
    for(const auto* id:{"tone","tone-label","sustain","sustain-label","vibrato","vibrato-label"})
        shell.removeChild(shell.getChildWithProperty("id",id),nullptr);
    for(const auto* id:{"gain","gain-label","pad","speaker","silence","footer"}) {
        auto node=shell.getChildWithProperty("id",id);
        if(node.isValid()) node.copyPropertiesFrom(newShell.getChildWithProperty("id",id),nullptr);
    }
    for(int i=0;i<36;++i) {
        const auto id="chord-"+juce::String(i);
        auto node=shell.getChildWithProperty("id",id);
        if(node.isValid()) node.copyPropertiesFrom(newShell.getChildWithProperty("id",id),nullptr);
    }
    shell.appendChild(newShell.getChildWithProperty("id","sound-controls").createCopy(),nullptr);
    auto types=layout.getChildWithName("Styles").getChild(0).getChildWithName("Types");
    if(!types.getChildWithName("ComboBox").isValid())
        types.appendChild(defaults.getChildWithName("Styles").getChild(0).getChildWithName("Types").getChildWithName("ComboBox").createCopy(),nullptr);
    root.setProperty("layout-version",2,nullptr);
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
    addFloat("harpAttack","Harp Attack",0.001f,0.08f,0.002f);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"harpQuantize",1},"Harp Chord Notes",true));
    addFloat("delayTime","Delay Time (ms)",40,1200,350);
    addFloat("delayFeedback","Delay Feedback",0,0.85f,0.3f);
    addFloat("delayMix","Delay Mix",0,1,0);
    addFloat("reverbSize","Reverb Size",0,1,0.5f);
    addFloat("reverbDamping","Reverb Damping",0,1,0.5f);
    addFloat("reverbMix","Reverb Mix",0,1,0);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"computerKeys",1},"Mac Keyboard",true));
    return p;
}
ChordHarpProcessor::ChordHarpProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"ChordHarp",parameters()) {
    const char* ids[]{"gain","tone","sustain","vibrato","chord","pad","harpGain","organGain",
        "organSound","organTone","organOctave","harpSound","harpAttack","harpQuantize",
        "delayTime","delayFeedback","delayMix","reverbSize","reverbDamping","reverbMix","computerKeys"};
    for(size_t i=0;i<raw.size();++i) raw[i]=state.getRawParameterValue(ids[i]);
    chordParameter=state.getParameter("chord");
    magicState.updateParameterMap();
    magicState.setGuiValueTree(BinaryData::magic_xml,BinaryData::magic_xmlSize);
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
    const int chord=static_cast<int>(state.getRawParameterValue("chord")->load());
    int a,n,b,m; fifo.prepareToWrite(1,a,n,b,m);
    if(n>0) notes[static_cast<size_t>(a)]=chordharp::chordNote(chord%12,chord/12,juce::jlimit(0,11,index));
    fifo.finishedWrite(n);
}
void ChordHarpProcessor::selectChord(int value) {
    chordParameter->beginChangeGesture();
    chordParameter->setValueNotifyingHost(chordParameter->convertTo0to1(static_cast<float>(juce::jlimit(0,35,value))));
    chordParameter->endChangeGesture();
}
int ChordHarpProcessor::selectedChord() const { return static_cast<int>(raw[Chord]->load()); }
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
    engine.harpSound=static_cast<int>(value(HarpSound)); engine.attackSeconds=value(HarpAttack);
    masterGain.setTargetValue(value(Gain));
    delay.milliseconds=value(DelayTime); delay.feedback=value(DelayFeedback); delay.mix=value(DelayMix);
    juce::Reverb::Parameters ambience;
    ambience.roomSize=value(ReverbSize); ambience.damping=value(ReverbDamping);
    ambience.wetLevel=value(ReverbMix); ambience.dryLevel=1-value(ReverbMix); ambience.width=1;
    reverb.setParameters(ambience);
    const auto silence=[this] { engine.reset(); delay.reset(); reverb.reset(); organSilenced=true; };
    int activeChord=selectedChord();
    const bool pad=value(Pad)>0.5f;
    if(activeChord!=lastChord || pad!=lastPad) organSilenced=false;
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
                    chordParameter->setValueNotifyingHost(chordParameter->convertTo0to1(static_cast<float>(chord)));
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
        float left=engine.next(),right=left;
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
        state.replaceState(restored);
    }
}
juce::AudioProcessorEditor* ChordHarpProcessor::createEditor() { return new ChordHarpEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordHarpProcessor; }
