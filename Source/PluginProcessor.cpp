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
juce::ValueTree upgradeLayout(juce::ValueTree layout) {
    removeLegacyLogoPickers(layout);
    const auto defaults=juce::ValueTree::fromXml(juce::String(BinaryData::magic_xml,static_cast<size_t>(BinaryData::magic_xmlSize)));
    auto root=layout.getChildWithName("View"),shell=findNode(root,"shell");
    if(!shell.isValid() || static_cast<int>(root.getProperty("layout-version",0))<6)return upgradeLayout(defaults.createCopy());
    if(static_cast<int>(root.getProperty("layout-version",0))<8) {
        auto controls=findNode(root,"sound-controls");
        controls.removeAllChildren(nullptr);
        for(auto page:findNode(defaults,"sound-controls"))controls.appendChild(page.createCopy(),nullptr);
    }
    auto types=layout.getChildWithName("Styles").getChild(0).getChildWithName("Types");
    for(const auto* type:{"SynthSelector","FMPanel","SamplePanel","RhythmPanel"})if(!types.getChildWithName(type).isValid())types.appendChild(defaults.getChildWithName("Styles").getChild(0).getChildWithName("Types").getChildWithName(type).createCopy(),nullptr);
    root.setProperty("layout-version",8,nullptr);omnichord::prepareThemes(layout);root.setProperty("background-color","ff292a2d",nullptr);return layout;
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
    addFloat("organTone","Organ Tone",0,1,0.4f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"organOctave",1},"Organ Octave",juce::StringArray{"-1","0","+1"},1));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"harpSound",1},"Harp Sound",juce::StringArray{"Clasico","FM"},0));
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
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"theme",1},"Colour Theme",omnichord::themeNames(),0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"chordSustain",1},"Chord Sustain",true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"tapeEnabled",1},"Tape",false));
    const auto addTape=[&](const char* id,const char* name,float initial) {
        auto attributes=juce::AudioParameterFloatAttributes().withLabel("%")
            .withStringFromValueFunction([](float x,int) { return juce::String(juce::roundToInt(x*100)); })
            .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue()*0.01f; });
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(0,1),initial,attributes));
    };
    addTape("tapeNoise","Tape Noise",0); addTape("tapeWow","Tape Wow",0.15f);
    addTape("tapeFlutter","Tape Flutter",0.1f); addTape("tapeDrive","Tape Drive",0.25f);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"organModel",1},"Organ Model",
        juce::StringArray{"Clasico","Reed","Suave","FM","Samples"},0));
    addFloat("reverbPredelay","Reverb Predelay (ms)",0,250,0);
    addFloat("reverbLow","Reverb Low EQ (dB)",-12,12,0); addFloat("reverbMid","Reverb Mid EQ (dB)",-12,12,0); addFloat("reverbHigh","Reverb High EQ (dB)",-12,12,0);
    addFloat("reverbEarly","Reverb Early Reflections",0,1,0);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delaySync",1},"Delay Sync",false));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"delayDivision",1},"Delay Note",juce::StringArray{"1/1","1/2","1/2t","1/2dot","1/4","1/4t","1/4dot","1/8","1/8t","1/8dot","1/16","1/16t","1/16dot","1/32","1/32t","1/32dot","1/64"},4));
    addFloat("delayTempo","Standalone Tempo (BPM)",20,400,120);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delayEnabled",1},"Delay Enabled",true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"reverbEnabled",1},"Reverb Enabled",true));
    for(const bool organ:{false,true}) {
        const juce::String prefix=organ?"organ":"harp",name=organ?"Organ ":"Harp ";
        p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{prefix+"Unison",1},name+"Unison Voices",1,7,1));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Detune",1},name+"Detune (cents)",0.f,40.f,6.f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Width",1},name+"Stereo Width",0.f,1.f,.7f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Drift",1},name+"Unison Drift",0.f,1.f,.15f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Cutoff",1},name+"Filter Cutoff (Hz)",juce::NormalisableRange<float>(20,20000,1,.25f),organ?4000.f:4100.f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{prefix+"Resonance",1},name+"Filter Resonance",.5f,8.f,.707f));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{prefix+"FilterType",1},name+"Filter Type",juce::StringArray{"LP","BP","HP"},0));
        const auto patch=chordharp::synthPatch(0,organ);
        p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{prefix+"Algorithm",1},name+"FM Algorithm",1,32,patch.algorithm+1));
        p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{prefix+"Feedback",1},name+"FM Feedback",0,7,patch.feedback));
        for(int i=0;i<6;++i) {
            const auto& o=patch.op[static_cast<size_t>(i)];const juce::String label=name+"OP"+juce::String(i+1)+" ";
            const auto add=[&](const char* field,const char* title,float low,float high,float initial,float skew=1) {
                p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{fmOperatorID(organ,i,field),1},label+title,juce::NormalisableRange<float>(low,high,0,skew),initial));
            };
            add("Ratio","Ratio",.125f,32,o.ratio,.4f);add("Fine","Fine (cents)",-50,50,0);
            add("Level","Level",0,1,o.level);add("Attack","Attack (ms)",.5f,4000,o.attack*1000,.3f);
            add("Decay","Decay (ms)",5,8000,o.decay*1000,.3f);add("Sustain","Sustain",0,1,o.sustain);
            add("Release","Release (ms)",5,8000,o.release*1000,.3f);add("Velocity","Velocity Sensitivity",0,1,o.velocity);
        }
    }
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"rhythmEnabled",1},"Rhythm Play",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"rhythmHost",1},"Rhythm Follow DAW",true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"rhythmUseLoop",1},"Rhythm Use Recorded Loop",false));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"rhythmLoop",1},"Rhythm Loop",0,127,0));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"rhythmPattern",1},"Rhythm Pattern",juce::StringArray{"Omni Pop","Disco","Bossa","Half-time","Shuffle","Waltz","Custom"},0));
    addFloat("rhythmSwing","Rhythm Swing",0,.6f,0);addFloat("rhythmVolume","Rhythm Volume",0,1,.7f);addFloat("rhythmSend","Rhythm FX Send",0,1,0);
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"organSample",1},"Organ Sample",0,127,0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"sampleRoot",1},"Sample Root MIDI Note",0,127,60));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sampleChord",1},"Sample Is Whole Chord",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sampleLoop",1},"Sample Loop",false));
    addFloat("sampleRelease","Sample Release (ms)",10,4000,250);
    for(int track=0;track<5;++track) {
        p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"rhythmSample"+juce::String(track),1},"Rhythm Track "+juce::String(track+1)+" Sample",0,128,0));
        for(int step=0;step<16;++step)p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{rhythmStepID(track,step),1},"Rhythm "+juce::String(track+1)+" Step "+juce::String(step+1),false));
    }
    return p;
}
ChordHarpProcessor::ChordHarpProcessor(juce::File presetRoot):AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"ChordHarp",parameters()),presetHome(presetRoot==juce::File{}?defaultPresetHome():presetRoot) {
    const char* ids[]{"gain","tone","sustain","vibrato","chord","pad","harpGain","organGain",
        "organTone","organOctave","harpSound","harpAttack","harpQuantize",
        "delayTime","delayFeedback","delayMix","reverbSize","reverbDamping","reverbMix","computerKeys","major7","theme","chordSustain","tapeEnabled","tapeNoise","tapeWow","tapeFlutter","tapeDrive","organModel","reverbPredelay","reverbLow","reverbMid","reverbHigh","reverbEarly","delaySync","delayDivision","delayTempo","delayEnabled","reverbEnabled"};
    for(size_t i=0;i<raw.size();++i) raw[i]=state.getRawParameterValue(ids[i]);
    static_assert(std::size(ids)==NumParams);
    const char* common[]{"Unison","Detune","Width","Drift","Cutoff","Resonance","FilterType","Algorithm","Feedback"};
    const char* fields[]{"Ratio","Fine","Level","Attack","Decay","Sustain","Release","Velocity"};
    for(size_t bank=0;bank<2;++bank) {
        const bool organ=bank==1;const juce::String prefix=organ?"organ":"harp";
        for(size_t i=0;i<9;++i)synthRefs[bank].common[i]=state.getRawParameterValue(prefix+common[i]);
        for(int op=0;op<6;++op)for(size_t i=0;i<8;++i)synthRefs[bank].op[static_cast<size_t>(op)][i]=state.getRawParameterValue(fmOperatorID(organ,op,fields[i]));
    }
    const char* rhythmIDs[]{"rhythmEnabled","rhythmHost","rhythmPattern","rhythmSwing","rhythmVolume","rhythmSend","organSample","sampleRoot","sampleRelease","rhythmUseLoop","rhythmLoop"};
    for(size_t i=0;i<rhythmRaw.size();++i)rhythmRaw[i]=state.getRawParameterValue(rhythmIDs[i]);
    const char* sampleIDs[]{"sampleChord","sampleLoop","organCutoff","organResonance","organFilterType"};
    for(size_t i=0;i<sampleRaw.size();++i)sampleRaw[i]=state.getRawParameterValue(sampleIDs[i]);
    for(int track=0;track<5;++track) {
        trackRaw[static_cast<size_t>(track)]=state.getRawParameterValue("rhythmSample"+juce::String(track));
        for(int step=0;step<16;++step)stepRaw[static_cast<size_t>(track)][static_cast<size_t>(step)]=state.getRawParameterValue(rhythmStepID(track,step));
    }
    chordParameter=state.getParameter("chord");
    major7Parameter=state.getParameter("major7");
    magicState.updateParameterMap();
    magicState.setGuiValueTree(upgradeLayout(juce::ValueTree::fromXml(juce::String(BinaryData::magic_xml,static_cast<size_t>(BinaryData::magic_xmlSize)))));
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    // Gives Save XML a useful starting directory for a local development build.
    const auto uiFile=juce::File(__FILE__).getParentDirectory().getSiblingFile("Assets").getChildFile("magic.xml");
    magicState.setResourcesFolder(uiFile.getFullPathName());
#endif
#if OMNICHORD_HAS_FACTORY_SAMPLES
    {
        juce::TemporaryFile bundledSamples(".zip");juce::String error;
        if(bundledSamples.getFile().replaceWithData(BinaryData::FactorySamples_zip,static_cast<size_t>(BinaryData::FactorySamples_zipSize))
            && loadSampleLibrary(bundledSamples.getFile(),error)) {
            factoryLibraryPath=sampleLibraryPath();
            // Start with an instrument selected in the sampler, rather than
            // the first alphabetically sorted drum loop. Sources stay OFF.
            if(!sampleLibrary->instruments.empty())selectSampleSound(sampleLibrary->instruments[0]);
            state.getParameter("organModel")->setValueNotifyingHost(0);
        }
    }
#endif
    factoryPreset=capturePreset("Inicio");
    loadStartupPreset();
    magicState.addTrigger("silence",[this] {
        auto* parameter=state.getParameter("pad");
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();
        auto* rhythmPlay=state.getParameter("rhythmEnabled");
        rhythmPlay->beginChangeGesture();rhythmPlay->setValueNotifyingHost(0);rhythmPlay->endChangeGesture();
        panic.store(true);
    });
}
void ChordHarpProcessor::prepareToPlay(double rate,int blockSize) {
    readSynthParameters();
    engine.prepare(rate);
    setLatencySamples(engine.latencySamples());
    rhythm.prepare(rate,engine.latencySamples());sampleChords.prepare(rate,engine.latencySamples());sampleFilter.prepare(rate);sampleDC.prepare(rate);
    rhythmMuted=false;lastRhythmEnabled=false;
    delay.milliseconds=raw[DelayTime]->load();
    delay.mix=raw[DelayEnabled]->load()>.5f?raw[DelayMix]->load():0;
    delay.prepare(rate);
    reverb.mix=raw[ReverbEnabled]->load()>.5f?raw[ReverbMix]->load():0; reverb.predelayMs=raw[ReverbPredelay]->load(); reverb.early=raw[ReverbEarly]->load();
    reverb.gainDb={raw[ReverbLow]->load(),raw[ReverbMid]->load(),raw[ReverbHigh]->load()}; reverb.prepare(rate);
    masterGain.reset(rate,0.02); masterGain.setCurrentAndTargetValue(raw[Gain]->load());
    organSilenced=false; lastChord=selectedChord(); lastPad=raw[Pad]->load()>0.5f;
    tape.enabled=raw[TapeEnabled]->load()>0.5f; tape.noiseLevel=raw[TapeNoise]->load();
    tape.wowAmount=raw[TapeWow]->load(); tape.flutterAmount=raw[TapeFlutter]->load(); tape.driveAmount=raw[TapeDrive]->load();
    tape.prepare(rate); tapeMuted=false;
    heldChords.clear(); guiChordKeys.reset(); guiChordMidi.clear(); guiChordMidi.ensureSize(16384);
    lastChordSustain=raw[ChordSustain]->load()>0.5f;
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
void ChordHarpProcessor::pressChord(int value,bool computer) {
    selectChord(value);
    guiChordKeys.noteOn(computer?2:1,chordharp::midiFromChord(value),0.8f);
}
void ChordHarpProcessor::releaseChord(int value,bool computer) {
    guiChordKeys.noteOff(computer?2:1,chordharp::midiFromChord(value),0);
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
    guiChordMidi.clear();
    guiChordKeys.processNextMidiBuffer(guiChordMidi,0,buffer.getNumSamples(),true);
    const auto value=[this](Param p) { return raw[p]->load(); };
    engine.gain=1;
    readSynthParameters();
    masterGain.setTargetValue(value(Gain));
    double bpm=value(DelayTempo); bool fromHost=false;
    bool hasPpq=false,hostPlaying=false;double ppq=0;
    if(auto* playhead=getPlayHead()) if(const auto position=playhead->getPosition()) {
        if(const auto hostBpm=position->getBpm()) if(std::isfinite(*hostBpm) && *hostBpm>0) {bpm=*hostBpm;fromHost=true;}
        if(const auto beat=position->getPpqPosition()) if(std::isfinite(*beat) && std::abs(*beat)<1e9) {hasPpq=true;ppq=*beat;}
        hostPlaying=position->getIsPlaying();
    }
    bpm=juce::jlimit(20.0,400.0,bpm); currentTempo.store(bpm); hostTempo.store(fromHost);
    readRhythmParameters(hasPpq,ppq,hostPlaying);
    delay.synced=value(DelaySync)>0.5f;
    delay.milliseconds=delay.synced?static_cast<float>(chordharp::syncedDelayMs(bpm,static_cast<int>(value(DelayDivision)))):value(DelayTime);
    delay.feedback=value(DelayFeedback); delay.mix=value(DelayEnabled)>.5f?value(DelayMix):0;
    reverb.roomSize=value(ReverbSize); reverb.damping=value(ReverbDamping); reverb.mix=value(ReverbEnabled)>.5f?value(ReverbMix):0;
    reverb.predelayMs=value(ReverbPredelay); reverb.early=value(ReverbEarly);
    reverb.gainDb={value(ReverbLow),value(ReverbMid),value(ReverbHigh)}; reverb.configure();
    tape.enabled=value(TapeEnabled)>0.5f; tape.noiseLevel=value(TapeNoise);
    tape.wowAmount=value(TapeWow); tape.flutterAmount=value(TapeFlutter); tape.driveAmount=value(TapeDrive);
    int activeChord=selectedChord(),held=heldChords.latest();
    const bool pad=value(Pad)>0.5f,sustain=value(ChordSustain)>0.5f;
    if(chordRetrigger.exchange(false) || activeChord!=lastChord || pad!=lastPad || sustain!=lastChordSustain) organSilenced=false;
    if(!sustain && held>=0) { activeChord=held; setChordFromMidi(held); }
    const auto refreshOrgan=[&] {
        const bool gate=pad && !organSilenced && (sustain || held>=0);
        engine.chord(activeChord%12,activeChord/12,gate);
        sampleChords.chord(activeChord%12,activeChord/12,gate && engine.organModel==4,engine.organOctave);
    };
    const auto silence=[&] {
        engine.reset(); delay.reset(); reverb.reset(); tape.reset(); heldChords.clear(); held=-1;
        organSilenced=true; tapeMuted=true;rhythmMuted=true;rhythm.enabled=false;rhythm.reset();sampleChords.reset();sampleFilter.reset();sampleDC.reset();
    };
    const bool stop=panic.exchange(false);
    const bool presetChanged=presetReset.exchange(false);
    if(stop || presetChanged) {
        silence();guiChordMidi.clear();
        if(presetChanged && !stop) {
            organSilenced=false;rhythmMuted=false;tapeMuted=false;lastRhythmEnabled=false;
            readRhythmParameters(hasPpq,ppq,hostPlaying);
        }
    }
    refreshOrgan();
    int a,n,b,m; fifo.prepareToRead(fifo.getNumReady(),a,n,b,m);
    if(!stop && !presetChanged) {
        if(n+m>0) tapeMuted=false;
        for(int i=0;i<n;++i) engine.pluck(notes[static_cast<size_t>(a+i)]);
        for(int i=0;i<m;++i) engine.pluck(notes[static_cast<size_t>(b+i)]);
    }
    fifo.finishedRead(n+m);
    const auto selector=[&](const juce::MidiMessage& message,bool gui) {
        const int chord=chordharp::chordFromMidi(message.getNoteNumber());
        if(chord<0) return;
        const int source=(gui?768:0)+(message.getChannel()-1)*48+message.getNoteNumber()-24;
        if(message.isNoteOn()) {
            heldChords.press(source,chord); held=chord; activeChord=chord;
            organSilenced=false; tapeMuted=false; setChordFromMidi(chord);
        } else if(message.isNoteOff()) {
            heldChords.release(source); held=heldChords.latest();
            if(!sustain && held>=0) { activeChord=held; setChordFromMidi(held); }
        }
    };
    auto event=midi.cbegin(),guiEvent=guiChordMidi.cbegin();
    for(int sample=0;sample<buffer.getNumSamples();++sample) {
        auto guiEnd=guiEvent;
        while(guiEnd!=guiChordMidi.cend() && (*guiEnd).samplePosition<=sample) ++guiEnd;
        for(auto it=guiEvent;it!=guiEnd;++it) selector((*it).getMessage(),true);
        guiEvent=guiEnd;
        auto groupEnd=event;
        while(groupEnd!=midi.cend() && (*groupEnd).samplePosition<=sample) ++groupEnd;
        auto arpBegin=event;
        // Selectors precede harp notes at the same timestamp. Preserve selector
        // on/off order, and discard harp notes that precede a panic in the group.
        for(auto it=event;it!=groupEnd;++it) {
            const auto message=(*it).getMessage();
            if(message.isNoteOn() || message.isNoteOff()) selector(message,false);
            if(message.isAllSoundOff() || message.isAllNotesOff()) { silence(); arpBegin=it; ++arpBegin; }
        }
        refreshOrgan();
        for(auto it=arpBegin;it!=groupEnd;++it) {
            const auto message=(*it).getMessage();
            if(message.isNoteOff() && message.getNoteNumber()>=72)engine.releaseHarp((message.getChannel()-1)*128+message.getNoteNumber());
            if(message.isNoteOn() && message.getNoteNumber()>=72) {
                tapeMuted=false;
                engine.pluck(chordharp::harpFromMidi(message.getNoteNumber(),activeChord,value(HarpQuantize)>0.5f),message.getFloatVelocity(),(message.getChannel()-1)*128+message.getNoteNumber());
            }
        }
        event=groupEnd;
        engine.next();
        if((sample&15)==0)sampleFilter.tick();
        const auto sampleAudio=sampleChords.next();
        const auto drums=rhythm.next();const float send=rhythmRaw[5]->load();
        float left=engine.leftOutput+sampleFilter.next(sampleDC.next(sampleAudio[0],0),0)+drums[0]*send;
        float right=engine.rightOutput+sampleFilter.next(sampleDC.next(sampleAudio[1],1),1)+drums[1]*send;
        delay.process(left,right);
        reverb.process(left,right);
        left+=drums[0]*(1-send);right+=drums[1]*(1-send);
        tape.process(left,right,!tapeMuted);
        const float master=masterGain.getNextValue();
        left=std::tanh(left*master); right=std::tanh(right*master);
        if(buffer.getNumChannels()==1) buffer.setSample(0,sample,(left+right)*0.5f);
        else { buffer.setSample(0,sample,left); buffer.setSample(1,sample,right); }
    }
    playingStep.store(rhythm.enabled?rhythm.lastStep:-1);
    lastChord=activeChord; lastPad=pad; lastChordSustain=sustain;
    midi.clear();
}
double ChordHarpProcessor::getTailLengthSeconds() const {
    if(!raw[DelayFeedback]) return 120;
    const double ms=raw[DelaySync]->load()>0.5f?chordharp::syncedDelayMs(currentTempo.load(),static_cast<int>(raw[DelayDivision]->load())):raw[DelayTime]->load()*1.13;
    const double feedback=raw[DelayFeedback]->load();
    return 80+ms/1000*(feedback>0.001?std::ceil(std::log(0.0001)/std::log(std::min(.85,feedback))):1);
}
void ChordHarpProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=state.copyState();
    snapshot.removeProperty("customLogo",nullptr);
    snapshot.setProperty("presetName",getPresetName(),nullptr);
    snapshot.setProperty("synthVersion",6,nullptr);
    snapshot.setProperty("sampleLibrary",sampleLibraryReference(),nullptr);
    snapshot.setProperty("guiLayout",magicState.getGuiTree().toXmlString(),nullptr);
    if(auto xml=snapshot.createXml()) copyXmlToBinary(*xml,dest);
}
void ChordHarpProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto restored=juce::ValueTree::fromXml(*xml);
        if(static_cast<int>(restored.getProperty("synthVersion",0))!=6)return;
        const auto samples=restored.getProperty("sampleLibrary").toString();
        if(samples.isNotEmpty()) {juce::String error;restoreSampleLibrary(samples,error);}
        restored.removeProperty("customLogo",nullptr);
        const auto layout=juce::ValueTree::fromXml(restored.getProperty("guiLayout").toString());
        if(layout.hasType("magic") && layout.getChildWithName("View").isValid())
            magicState.setGuiValueTree(upgradeLayout(layout));
        state.replaceState(restored);
        updatePresetName(restored.getProperty("presetName","Proyecto").toString());
    }
}
juce::AudioProcessorEditor* ChordHarpProcessor::createEditor() { return new ChordHarpEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordHarpProcessor; }
