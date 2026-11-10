#pragma once

static void checkGraySlateSamples() {
    const auto source=juce::File(__FILE__).getParentDirectory().getParentDirectory().getChildFile("samples");
    const auto packed=juce::File::getCurrentWorkingDirectory();
    require(source.getChildFile("grayslate-index.json").copyFileTo(packed.getChildFile("grayslate-index.json")),"Copy the catalog next to the generated archive for the installed-resource test");
    SampleLibrary catalog;juce::String error;
    require(catalog.appendGraySlate(packed,error) && catalog.entries.size()==477,"All original GraySlate recordings must be indexed from the packaged bank");
    for(const auto& entry:catalog.entries)require(entry.audio.frames()==0,"Catalog enumeration must not decode hundreds of recordings");
    int floating64=0;
    for(const auto& entry:catalog.entries) {
        chordharp::SampleAudio decoded;
        if(!SampleLibrary::decodeRecording(entry.recording,decoded,error,entry.archive))std::cerr<<entry.name<<": "<<error<<'\n';
        require(decoded.frames()>0 && decoded.rate==96000,"Every packaged WAV, including IEEE float64, must decode at its original rate");
        float peak=0;for(const auto& c:decoded.channel)for(float x:c){require(std::isfinite(x),"Decoded samples must be finite");peak=std::max(peak,std::abs(x));}
        require(peak>0,"Every recording must contain real audio");
        const auto original=source.getChildFile("GraySlate's Omnichord Pack").getChildFile(entry.path.fromFirstOccurrenceOf("GraySlate/",false,false));
        auto stream=original.createInputStream();stream->setPosition(20);if(stream->readShort()==3)++floating64;
    }
    require(floating64==103,"The regression test must cover all 103 float64 originals");
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Omnichord GraySlate checks","",true);
    struct Cleanup {juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    ChordHarpProcessor synth(root,packed);
    require(synth.sampleNames().size()==492 && synth.sampleChoices().size()==482,"The new recordings must be appended to the original 15 entries");
    const auto find=[&](const char* suffix){for(const auto& choice:synth.sampleChoices())if(choice.second==suffix)return choice.first;return -1;};
    const int chord=find("GraySlate / C / Chord"),harp=find("GraySlate / C / Harp 00"),last=491;
    require(chord>14 && harp>14,"Both full chords and individual harp recordings must be selectable");
    synth.selectSampleSound(chord);require(synth.state.getRawParameterValue("sampleChord")->load()==1 && synth.state.getRawParameterValue("sampleRoot")->load()==48,"Full chord recordings must select their real root and whole-chord mode");
    synth.selectSampleSound(harp);require(synth.state.getRawParameterValue("sampleChord")->load()==0 && synth.state.getRawParameterValue("sampleRoot")->load()==60,"Harp one-shots must build the selected chord from their measured root");
    synth.selectSampleSound(last);require(synth.state.getRawParameterValue("organSample")->load()==last,"Samples beyond the former 127 limit must be selectable");
    synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,24,.9f),0);float peak=0;
    for(int b=0;b<60;++b){synth.processBlock(audio,midi);peak=std::max(peak,audio.getMagnitude(0,512));}require(peak>.001f,"A late GraySlate recording must produce audio through the actual processor");
    const auto preset=root.getChildFile("GraySlate.omnipreset");require(synth.savePreset(preset,error),"A selected recording must save in a complete preset");
    ChordHarpProcessor restored(root.getChildFile("other-account"),packed);require(restored.loadPreset(preset,error) && restored.sampleNames().size()==492 && restored.state.getRawParameterValue("organSample")->load()==last,"Presets must recall the portable factory bank across cache locations");
    juce::MemoryBlock state;synth.getStateInformation(state);restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    require(restored.state.getRawParameterValue("organSample")->load()==last,"DAW state must recall a late bank entry too");
    std::cout<<"GraySlate: all 477 recordings decoded, 103 float64, portable preset/state and real audio passed\n";
}
