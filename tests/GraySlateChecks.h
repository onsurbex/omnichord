#pragma once
#include "../Source/KeyboardMapping.h"

static void checkGraySlateSamples() {
    const auto source=juce::File(__FILE__).getParentDirectory().getParentDirectory().getChildFile("samples");
    const auto packed=juce::File::getCurrentWorkingDirectory();
    require(source.getChildFile("grayslate-index.json").copyFileTo(packed.getChildFile("grayslate-index.json")),"Copy the catalog next to the generated archive for the installed-resource test");
    SampleLibrary catalog;juce::String error;
    require(catalog.appendGraySlate(packed,error) && catalog.entries.size()==477,"All original GraySlate recordings must be indexed from the packaged bank");
    for(const auto& entry:catalog.entries)require(entry.audio.frames()==0,"Catalog enumeration must not decode hundreds of recordings");
    const auto chords=SampleLibrary::graySlateBank(packed,false,error),harps=SampleLibrary::graySlateBank(packed,true,error);
    require(chords && harps,"Both GraySlate banks must preload outside the audio callback");
    const juce::StringArray rootNames{"c","db","d","eb","e","f","f#","g","ab","a","bb","b"};
    int floating64=0;
    for(const auto& entry:catalog.entries) {
        chordharp::SampleAudio decoded;
        if(!SampleLibrary::decodeRecording(entry.recording,decoded,error,entry.archive))std::cerr<<entry.name<<": "<<error<<'\n';
        require(decoded.frames()>0 && decoded.rate==96000,"Every packaged WAV, including IEEE float64, must decode at its original rate");
        float peak=0;for(const auto& c:decoded.channel)for(float x:c){require(std::isfinite(x),"Decoded samples must be finite");peak=std::max(peak,std::abs(x));}
        require(peak>0,"Every recording must contain real audio");
        const auto folder=entry.recording.getParentDirectory().getFileName();
        if(folder!="rhythm") {
            const int quality=folder.endsWith("m")?1:folder.endsWith("7")?2:0;
            const int root=rootNames.indexOf(quality==0?folder:folder.dropLastCharacters(1));require(root>=0,"Each source folder must name a musical root");
            const auto* mapped=entry.wholeChord?chords->chord(root,quality):&harps->harps[static_cast<size_t>(quality*12+root)][static_cast<size_t>(entry.recording.getFileNameWithoutExtension().substring(folder.length()).getIntValue())];
            require(mapped && mapped->frames()==decoded.frames(),"Every source WAV must occupy its matching chord or numbered harp slot");
            for(size_t frame=0;frame<std::min<size_t>(decoded.frames(),4096);frame+=7)for(size_t c=0;c<2;++c)require(std::abs(mapped->read(c,static_cast<double>(frame))-decoded.read(c,static_cast<double>(frame)))<1e-6f,"Bank routing and compact storage must preserve the matching original WAV audio");
        }
        const auto original=source.getChildFile("GraySlate's Omnichord Pack").getChildFile(entry.path.fromFirstOccurrenceOf("GraySlate/",false,false));
        auto stream=original.createInputStream();stream->setPosition(20);if(stream->readShort()==3)++floating64;
    }
    require(floating64==103,"The regression test must cover all 103 float64 originals");

    require(chords==SampleLibrary::graySlateBank(packed,false,error) && harps==SampleLibrary::graySlateBank(packed,true,error),"Plugin instances must share immutable bank data");
    require(chords->storageBytes()+harps->storageBytes()<941659308,"Compact shared storage must stay below the original WAV footprint");
    for(int q=0;q<3;++q)for(int r=0;r<12;++r) {
        const auto* original=chords->chord(r,q);require(original && original->frames()>0,"Each chord must have its own recording");
        chordharp::GraySlatePlayer actual;actual.bank=chords.get();actual.loop=false;actual.prepare(96000,0);actual.chord(r,q,true,0);
        chordharp::SampleVoice expected;expected.start(original,96000,1,original->gain,false);
        for(int i=0;i<1024;++i){const auto a=actual.next(),b=expected.next(96000);require(std::abs(a[0]-b[0])<1e-6f && std::abs(a[1]-b[1])<1e-6f,"Every chord must play exactly one correct recording at original pitch, not build a triad from it");}
        for(int step=0;step<12;++step) {
            const auto& recording=harps->harps[static_cast<size_t>(q*12+r)][static_cast<size_t>(step)];
            if(recording.frames()==0){require(q==0 && r==10 && step==11,"Only the supplied Bb step 11 may be missing");continue;}
            const auto pick=harps->step(r,q,step);require(pick.audio==&recording && pick.pitch==1,"All 431 supplied harp positions must use their exact recording without pitch changes");
            chordharp::GraySlatePlayer player;player.bank=harps.get();player.prepare(96000,0);player.pluck(r,q,step,1);
            chordharp::SampleVoice reference;reference.start(&recording,96000,1,recording.gain,false);
            for(int i=0;i<512;++i){const auto a=player.next(),b=reference.next(96000);require(std::abs(a[0]-b[0])<1e-6f && std::abs(a[1]-b[1])<1e-6f,"A harp stroke must reproduce the selected chord's exact numbered recording");}
            require(harps->step(r,q,step+12).pitch==2,"Notes beyond the twelve recorded positions must transpose the matching source");
        }
    }
    require(harps->step(10,0,11).audio && harps->step(10,0,11).pitch>1,"The absent Bb harp position must use a pitched neighbouring recording");
    require(chords->chord(0,3)==chords->chord(0,0) && chords->majorSeventh(0,0).audio,"Maj7 must combine the major recording with a sampled seventh");
    require(harps->step(0,3,2).pitch>1 && harps->step(0,3,0).pitch==1,"Maj7 harp fallback must only raise the recorded dominant seventh");

    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Omnichord GraySlate checks","",true);
    struct Cleanup {juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    ChordHarpProcessor synth(root,packed);
    const auto choices=synth.sampleChoices();require(synth.sampleNames().size()==492 && choices.size()==6,"Samples must expose five legacy sounds and one aggregated GraySlate bank");
    int bank=-1,count=0;for(const auto& choice:choices)if(choice.second=="Grayslate samples"){bank=choice.first;++count;}
    require(count==1 && bank==15,"The aggregated choice must preserve legacy sample indices");
    synth.selectSampleSound(bank);require(synth.graySlateSamples() && synth.currentSampleChoice()==bank,"Selecting the bank must enable automatic chord selection");
    const auto harpNames=synth.synthSoundNames(false);require(harpNames.indexOf("Grayslate samples")==harpNames.size()-1,"Harp must expose a single GraySlate option alongside synthesis");
    synth.selectSynthSound(false,1);synth.selectSynthSound(false,harpNames.size()-1);require(synth.graySlateHarp() && synth.getSynthChoice(false)==harpNames.size()-1,"Harp bank selection must persist as a sound choice");
    synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    setActual(synth,"chordSustain",0);float peak=0;
    for(int chord:{0,12,24,7,19,31,36}) {
        midi.addEvent(juce::MidiMessage::noteOn(1,chordharp::midiFromChord(chord),.9f),0);
        midi.addEvent(juce::MidiMessage::noteOn(1,72,.8f),64);
        for(int b=0;b<12;++b){synth.processBlock(audio,midi);peak=std::max(peak,audio.getMagnitude(0,512));}
        midi.addEvent(juce::MidiMessage::noteOff(1,chordharp::midiFromChord(chord)),0);midi.addEvent(juce::MidiMessage::noteOff(1,72),0);synth.processBlock(audio,midi);
        require(synth.currentSampleChoice()==bank,"MIDI chord changes must leave the single bank option selected");
    }
    require(peak>.001f,"Chord and harp banks must render real audio under MIDI chord changes");
    synth.strum(11);synth.processBlock(audio,midi);require(audio.getMagnitude(0,512)>.0001f,"The mouse/Mac strum path must use the recorded harp too");
    const auto preset=root.getChildFile("GraySlate.omnipreset");require(synth.savePreset(preset,error),"Both bank selections must save in a complete preset");
    ChordHarpProcessor restored(root.getChildFile("other-account"),packed);restored.prepareToPlay(48000,512);require(restored.loadPreset(preset,error) && restored.graySlateSamples() && restored.graySlateHarp(),"Presets must restore both portable banks across cache locations");
    setActual(restored,"samplesEnabled",0);restored.processBlock(audio,midi);
    midi.addEvent(juce::MidiMessage::noteOn(1,72,.8f),0);float restoredHarp=0;for(int i=0;i<8;++i){restored.processBlock(audio,midi);restoredHarp=std::max(restoredHarp,audio.getMagnitude(0,512));}
    require(restoredHarp>.001f,"A preset must preload the selected harp before returning, without waiting for a message-loop timer");
    juce::MemoryBlock state;synth.getStateInformation(state);restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));require(restored.graySlateSamples() && restored.graySlateHarp(),"DAW state must restore both bank choices");
    synth.selectSampleSound(491);require(synth.currentSampleChoice()==bank,"Old 0.7.4 individual GraySlate indices must resolve to the aggregated bank");
    synth.selectSynthSound(false,0);require(!synth.graySlateHarp(),"Returning to a synthesized harp must disable recorded playback");

    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());auto* magic=dynamic_cast<ChordHarpEditor*>(editor.get());require(magic!=nullptr,"Bank editor must instantiate");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);auto& builder=magic->getGUIBuilder();
    auto* harpChoice=dynamic_cast<juce::ComboBox*>(builder.findGuiItemWithId("harpSound")->getWrappedComponent());
    auto* sampleChoice=dynamic_cast<juce::ComboBox*>(findComponent(*editor,"sample-sound"));
    require(harpChoice && sampleChoice && harpChoice->getText()=="Grayslate samples" && sampleChoice->getText()=="Grayslate samples","Both real GUI selectors must display the single aggregated sound");
    auto* target=dynamic_cast<juce::ComboBox*>(findComponent(*editor,"fm-target"));auto* enabled=dynamic_cast<juce::TextButton*>(findComponent(*editor,"fm-enabled"));
    require(target && enabled,"FM source controls must exist");target->setSelectedId(1,juce::sendNotificationSync);juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
    require(!enabled->getToggleState(),"Harp must not display FM as active when the recorded bank overrides the previous FM patch");
    auto* panel=builder.findGuiItemWithId("sound-controls");auto* tabs=findTabBar(*panel);require(tabs!=nullptr,"Bank previews need the main tabs");
    for(const auto& page:std::array<std::pair<int,const char*>,2>{{{1,"harp"},{4,"samples"}}}) {
        tabs->setCurrentTabIndex(page.first);juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
        auto output=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String("Omnichord-grayslate-")+page.second+"-panel-preview.png").createOutputStream();juce::PNGImageFormat png;
        require(output && png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds()),*output),"Both bank tabs must render with their selected sound");
    }
    std::cout<<"GraySlate banks: 36 exact chords, 431 exact harp positions, pitched fallback, cache sharing and MIDI/preset/GUI checks passed\n";
}
