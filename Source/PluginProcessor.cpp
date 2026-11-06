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
    for(const auto* name:{"OmniChord","OmniToggle","OmniSwitch","ThemeSelector","ComboBox"})
        if(!types.getChildWithName(name).isValid()) types.appendChild(defaults.getChildWithName("Styles").getChild(0).getChildWithName("Types").getChildWithName(name).createCopy(),nullptr);
    if(static_cast<int>(root.getProperty("layout-version",0))<4) {
        const auto switchNode=[&](const char* id,const char* text) {
            auto node=findNode(root,id); const auto model=findNode(defaults,id);
            if(!node.isValid()) { node=model.createCopy(); shell.appendChild(node,nullptr); return; }
            juce::ValueTree replacement("OmniSwitch"); replacement.copyPropertiesAndChildrenFrom(node,nullptr);
            for(const auto* property:{"pos-x","pos-y","pos-width","pos-height","switch-fixed-color","button-color","button-on-color","button-off-text","button-on-text"})
                if(model.hasProperty(property)) replacement.setProperty(property,model.getProperty(property),nullptr);
            replacement.setProperty("text",text,nullptr);
            node.getParent().removeChild(node,nullptr); shell.appendChild(replacement,nullptr);
        };
        switchNode("pad","ORGAN"); switchNode("harpQuantize","FOLLOW CHORDS"); switchNode("chordSustain","SUSTAIN");
        auto display=findNode(root,"chord-display");
        display.setProperty("pos-height",60,nullptr); display.setProperty("display-font-size",24,nullptr);
    }
    auto tabs=findNode(root,"sound-controls");
    if(!findNode(root,"tape-controls").isValid()) tabs.addChild(findNode(defaults,"tape-controls").createCopy(),juce::jmin(4,tabs.getNumChildren()),nullptr);
    migrateButtonTypes(root);
    if(static_cast<int>(root.getProperty("layout-version",0))<5) {
        root.setProperty("height",static_cast<int>(root.getProperty("height",695))+110,nullptr);
        shell.setProperty("pos-y",static_cast<int>(shell.getProperty("pos-y",15))+60,nullptr);
        shell.setProperty("pos-height",static_cast<int>(shell.getProperty("pos-height",665))+50,nullptr);
        for(const auto* id:{"harp-frame","strum"}) {
            auto node=findNode(root,id); node.setProperty("pos-height",static_cast<int>(node.getProperty("pos-height"))+50,nullptr);
        }
        for(const auto* id:{"strum-instructions","silence","footer"}) {
            auto node=findNode(root,id);node.setProperty("pos-y",static_cast<int>(node.getProperty("pos-y"))+50,nullptr);
        }
        tabs.setProperty("pos-height",225,nullptr);
        for(const auto* id:{"reverb-controls","delay-controls"}) {
            auto old=findNode(root,id); const int index=tabs.indexOf(old); tabs.removeChild(old,nullptr);
            tabs.addChild(findNode(defaults,id).createCopy(),index,nullptr);
        }
        auto sound=findNode(root,"organSound");sound.setProperty("parameter","organModel",nullptr);
        for(const auto* id:{"organRotary","organRotary-label","organPercussion","organPercussion-label"})
            if(!findNode(root,id).isValid()) findNode(root,"organ-controls").appendChild(findNode(defaults,id).createCopy(),nullptr);
    }
    if(!findNode(root,"instrument-body").isValid()) {
        auto body=findNode(defaults,"instrument-body").createCopy();
        body.setProperty("pos-width",shell.getProperty("pos-width"),nullptr);body.setProperty("pos-height",shell.getProperty("pos-height"),nullptr);
        shell.addChild(body,0,nullptr);
    }
    if(!findNode(root,"body-shadow").isValid()) {
        auto shadow=findNode(defaults,"body-shadow").createCopy();
        shadow.setProperty("pos-x",static_cast<int>(shell.getProperty("pos-x"))-12,nullptr);shadow.setProperty("pos-y",static_cast<int>(shell.getProperty("pos-y"))-12,nullptr);
        shadow.setProperty("pos-width",static_cast<int>(shell.getProperty("pos-width"))+24,nullptr);shadow.setProperty("pos-height",static_cast<int>(shell.getProperty("pos-height"))+24,nullptr);
        root.addChild(shadow,0,nullptr);
    }
    if(!findNode(root,"preset-bar").isValid()) root.appendChild(findNode(defaults,"preset-bar").createCopy(),nullptr);
    if(static_cast<int>(root.getProperty("layout-version",0))<6) {
        const auto copyBounds=[&](juce::ValueTree node,const juce::ValueTree& model) {
            for(const auto* key:{"pos-x","pos-y","pos-width","pos-height"})
                if(model.hasProperty(key)) node.setProperty(key,model.getProperty(key),nullptr);
        };
        copyBounds(tabs,findNode(defaults,"sound-controls"));
        for(const auto* key:{"tab-height","padding","border","border-color","radius"})
            tabs.setProperty(key,findNode(defaults,"sound-controls").getProperty(key),nullptr);
        for(auto control:findNode(defaults,"reverb-controls")) copyBounds(findNode(root,control.getProperty("id").toString()),control);
        for(const auto* id:{"organRotary","organRotary-label","organPercussion","organPercussion-label","delay-help","silence","footer","divider-439"})
            copyBounds(findNode(root,id),findNode(defaults,id));
        for(auto label:findNode(root,"keyboard-controls"))
            if(static_cast<int>(label.getProperty("pos-width"))>525) label.setProperty("pos-width",525,nullptr);
    }
    for(const auto* id:{"delayEnabled","reverbEnabled"})
        if(!findNode(root,id).isValid()) shell.appendChild(findNode(defaults,id).createCopy(),nullptr);
    root.setProperty("layout-version",6,nullptr);
    omnichord::prepareThemes(layout);
    root.setProperty("background-color","ff292a2d",nullptr);
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
        juce::StringArray{"Clasico (sesion)","Reed","Suave","Brillante","Iglesia Principal","Iglesia Pleno","Catedral","Flauta liturgica","Tonewheel Jazz","Tonewheel Gospel","Tonewheel Rock"},4));
    addFloat("organRotary","Organ Rotary",0,1,.35f); addFloat("organPercussion","Organ Percussion",0,1,.5f);
    addFloat("reverbPredelay","Reverb Predelay (ms)",0,250,0);
    addFloat("reverbLow","Reverb Low EQ (dB)",-12,12,0); addFloat("reverbMid","Reverb Mid EQ (dB)",-12,12,0); addFloat("reverbHigh","Reverb High EQ (dB)",-12,12,0);
    addFloat("reverbEarly","Reverb Early Reflections",0,1,0);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delaySync",1},"Delay Sync",false));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"delayDivision",1},"Delay Note",juce::StringArray{"1/1","1/2","1/2t","1/2dot","1/4","1/4t","1/4dot","1/8","1/8t","1/8dot","1/16","1/16t","1/16dot","1/32","1/32t","1/32dot","1/64"},4));
    addFloat("delayTempo","Standalone Tempo (BPM)",20,400,120);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"delayEnabled",1},"Delay Enabled",true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"reverbEnabled",1},"Reverb Enabled",true));
    return p;
}
ChordHarpProcessor::ChordHarpProcessor(juce::File presetRoot):AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"ChordHarp",parameters()),presetHome(presetRoot==juce::File{}?defaultPresetHome():presetRoot) {
    const char* ids[]{"gain","tone","sustain","vibrato","chord","pad","harpGain","organGain",
        "organSound","organTone","organOctave","harpSound","harpAttack","harpQuantize",
        "delayTime","delayFeedback","delayMix","reverbSize","reverbDamping","reverbMix","computerKeys","major7","organSpread","organChorus","theme","chordSustain","tapeEnabled","tapeNoise","tapeWow","tapeFlutter","tapeDrive","organModel","organRotary","organPercussion","reverbPredelay","reverbLow","reverbMid","reverbHigh","reverbEarly","delaySync","delayDivision","delayTempo","delayEnabled","reverbEnabled"};
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
    factoryPreset=capturePreset("Inicio");
    loadStartupPreset();
    magicState.addTrigger("silence",[this] {
        auto* parameter=state.getParameter("pad");
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();
        panic.store(true);
    });
}
void ChordHarpProcessor::prepareToPlay(double rate,int blockSize) {
    engine.organModel=static_cast<int>(raw[OrganModel]->load());
    engine.prepare(rate);
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
    engine.tone=value(Tone); engine.sustain=value(Sustain); engine.vibrato=value(Vibrato);
    engine.harpGain=value(HarpGain); engine.organGain=value(OrganGain);
    engine.organModel=static_cast<int>(value(OrganModel));
    engine.organSound=static_cast<int>(value(OrganSound)); engine.organTone=value(OrganTone);
    engine.organOctave=static_cast<int>(value(OrganOctave))-1;
    engine.organRotary=value(OrganRotary); engine.organPercussion=value(OrganPercussion);
    engine.organSpread=value(OrganSpread); engine.organChorus=value(OrganChorus);
    engine.harpSound=static_cast<int>(value(HarpSound)); engine.attackSeconds=value(HarpAttack)*0.001f;
    masterGain.setTargetValue(value(Gain));
    double bpm=value(DelayTempo); bool fromHost=false;
    if(auto* playhead=getPlayHead()) if(const auto position=playhead->getPosition()) if(const auto hostBpm=position->getBpm()) {
        if(std::isfinite(*hostBpm) && *hostBpm>0) { bpm=*hostBpm; fromHost=true; }
    }
    bpm=juce::jlimit(20.0,400.0,bpm); currentTempo.store(bpm); hostTempo.store(fromHost);
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
    const auto refreshOrgan=[&] { engine.chord(activeChord%12,activeChord/12,pad && !organSilenced && (sustain || held>=0)); };
    const auto silence=[&] {
        engine.reset(); delay.reset(); reverb.reset(); tape.reset(); heldChords.clear(); held=-1;
        organSilenced=true; tapeMuted=true;
    };
    const bool stop=panic.exchange(false);
    if(stop) { silence(); guiChordMidi.clear(); }
    refreshOrgan();
    int a,n,b,m; fifo.prepareToRead(fifo.getNumReady(),a,n,b,m);
    if(!stop) {
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
            if(message.isNoteOn() && message.getNoteNumber()>=72) {
                tapeMuted=false;
                engine.pluck(chordharp::harpFromMidi(message.getNoteNumber(),activeChord,value(HarpQuantize)>0.5f),message.getFloatVelocity());
            }
        }
        event=groupEnd;
        engine.next();
        float left=engine.leftOutput,right=engine.rightOutput;
        delay.process(left,right);
        reverb.process(left,right);
        tape.process(left,right,!tapeMuted);
        const float master=masterGain.getNextValue();
        left=std::tanh(left*master); right=std::tanh(right*master);
        if(buffer.getNumChannels()==1) buffer.setSample(0,sample,(left+right)*0.5f);
        else { buffer.setSample(0,sample,left); buffer.setSample(1,sample,right); }
    }
    lastChord=activeChord; lastPad=pad; lastChordSustain=sustain;
    midi.clear();
}
double ChordHarpProcessor::getTailLengthSeconds() const {
    if(!raw[DelayFeedback]) return 120;
    const double ms=raw[DelaySync]->load()>0.5f?chordharp::syncedDelayMs(currentTempo.load(),static_cast<int>(raw[DelayDivision]->load())):raw[DelayTime]->load()*1.13;
    const double feedback=raw[DelayFeedback]->load();
    return 30+ms/1000*(feedback>0.001?std::ceil(std::log(0.0001)/std::log(std::min(.85,feedback))):1);
}
void ChordHarpProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=state.copyState();
    snapshot.removeProperty("customLogo",nullptr);
    snapshot.setProperty("presetName",getPresetName(),nullptr);
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
        for(const auto* id:{"major7","organSpread","organChorus","theme","chordSustain","tapeEnabled","tapeNoise","tapeWow","tapeFlutter","tapeDrive","organRotary","organPercussion","reverbPredelay","reverbLow","reverbMid","reverbHigh","reverbEarly","delaySync","delayDivision","delayTempo","delayEnabled","reverbEnabled"}) {
            auto* parameter=state.getParameter(id);
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
        state.getParameter("organModel")->setValueNotifyingHost(0); // Sessions before v0.5 retain their exact legacy organ.
        state.replaceState(restored);
        updatePresetName(restored.getProperty("presetName","Proyecto").toString());
    }
}
juce::AudioProcessorEditor* ChordHarpProcessor::createEditor() { return new ChordHarpEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordHarpProcessor; }
