#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

juce::AudioProcessorValueTreeState::ParameterLayout ChordHarpProcessor::parameters() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1},"Volume",0.0f,1.0f,0.65f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"tone",1},"Tone",0.0f,1.0f,0.35f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain",1},"Sustain",juce::NormalisableRange<float>(0.15f,5.0f,0.01f,0.5f),1.6f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"vibrato",1},"Vibrato",0.0f,1.0f,0.12f));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"chord",1},"Chord",0,35,0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"pad",1},"Chord organ",false));
    return p;
}
ChordHarpProcessor::ChordHarpProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"ChordHarp",parameters()) {
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
void ChordHarpProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    magicState.processMidiBuffer(midi,buffer.getNumSamples());
    engine.gain=state.getRawParameterValue("gain")->load();
    engine.tone=state.getRawParameterValue("tone")->load();
    engine.sustain=state.getRawParameterValue("sustain")->load();
    engine.vibrato=state.getRawParameterValue("vibrato")->load();
    const bool stop=panic.exchange(false);
    if(stop) engine.reset();
    const int chord=static_cast<int>(state.getRawParameterValue("chord")->load());
    engine.chord(chord%12,chord/12,!stop && state.getRawParameterValue("pad")->load()>0.5f);
    int a,n,b,m; fifo.prepareToRead(fifo.getNumReady(),a,n,b,m);
    if(!stop) {
        for(int i=0;i<n;++i) engine.pluck(notes[static_cast<size_t>(a+i)]);
        for(int i=0;i<m;++i) engine.pluck(notes[static_cast<size_t>(b+i)]);
    }
    fifo.finishedRead(n+m);
    auto event=midi.cbegin();
    for(int sample=0;sample<buffer.getNumSamples();++sample) {
        while(event!=midi.cend() && (*event).samplePosition<=sample) {
            const auto message=(*event).getMessage();
            if(message.isNoteOn()) engine.pluck(message.getNoteNumber(),message.getFloatVelocity());
            if(message.isAllSoundOff() || message.isAllNotesOff()) engine.reset();
            ++event;
        }
        const float value=engine.next();
        for(int channel=0;channel<buffer.getNumChannels();++channel) buffer.setSample(channel,sample,value);
    }
    midi.clear();
}
void ChordHarpProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=state.copyState();
    snapshot.setProperty("guiLayout",magicState.getGuiTree().toXmlString(),nullptr);
    if(auto xml=snapshot.createXml()) copyXmlToBinary(*xml,dest);
}
void ChordHarpProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto restored=juce::ValueTree::fromXml(*xml);
        const auto layout=juce::ValueTree::fromXml(restored.getProperty("guiLayout").toString());
        if(layout.hasType("magic") && layout.getChildWithName("View").isValid())
            magicState.setGuiValueTree(layout);
        state.replaceState(restored);
    }
}
juce::AudioProcessorEditor* ChordHarpProcessor::createEditor() { return new ChordHarpEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordHarpProcessor; }
