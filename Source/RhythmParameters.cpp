#include "PluginProcessor.h"

juce::String ChordHarpProcessor::rhythmStepID(int track,int step) {return "rhythmStep"+juce::String(track)+"_"+juce::String(step);}
void ChordHarpProcessor::editRhythmStep(int track,int step,bool on) {
    if(track<0 || track>=5 || step<0 || step>=16)return;
    const auto set=[&](const juce::String& id,float value){auto* p=state.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();};
    const int pattern=static_cast<int>(rhythmRaw[2]->load());
    if(pattern<6) {
        for(int t=0;t<5;++t)for(int s=0;s<16;++s)set(rhythmStepID(t,s),(chordharp::rhythmPatterns[static_cast<size_t>(pattern)][static_cast<size_t>(t)]&(1u<<s))!=0?1.f:0.f);
        set("rhythmPattern",6);
    }
    set(rhythmStepID(track,step),on?1.f:0.f);
}
juce::StringArray ChordHarpProcessor::sampleNames() const {
    const juce::ScopedLock lock(libraryLock);juce::StringArray names;
    if(sampleLibrary)for(const auto& sound:sampleLibrary->entries)names.add(sound.name);return names;
}
juce::String ChordHarpProcessor::sampleLibraryPath() const {const juce::ScopedLock lock(libraryLock);return sampleLibrary?sampleLibrary->source:juce::String{};}
bool ChordHarpProcessor::loadSampleLibrary(const juce::File& file,juce::String& error) {
    auto decoded=SampleLibrary::readZip(file,error);if(!decoded)return false;
    // Cache an immutable copy so a moved download does not break sessions/presets.
    auto stream=file.createInputStream();if(!stream){error="No se ha podido abrir el ZIP.";return false;}
    juce::uint64 hash=14695981039346656037ull;std::array<unsigned char,16384> block{};
    while(const int count=stream->read(block.data(),static_cast<int>(block.size()))) {
        if(count<0){error="Error leyendo el ZIP.";return false;}
        for(int i=0;i<count;++i){hash^=block[static_cast<size_t>(i)];hash*=1099511628211ull;}
    }
    const auto cache=presetHome.getChildFile("Samples").getChildFile(juce::String::toHexString(static_cast<juce::int64>(hash))+".zip");
    if(!cache.getParentDirectory().createDirectory().wasOk() || (!cache.existsAsFile() && !file.copyFileTo(cache))) {error="No se ha podido guardar la libreria de samples.";return false;}
    decoded->source=cache.getFullPathName();
    {
        // Decode and copy outside the callback lock. Swap only while processing
        // is excluded by JUCE's host/standalone wrappers; no file I/O on audio.
        const juce::ScopedLock audioLock(getCallbackLock());
        const juce::ScopedLock metadataLock(libraryLock);
        sampleChords.reset();rhythm.reset();sampleFilter.reset();sampleDC.reset();
        rhythm.samples={};sampleChords.sound=nullptr;
        sampleLibrary.swap(decoded);++sampleRevision;
    }
    error.clear();return true;
}
void ChordHarpProcessor::readRhythmParameters(bool hasPpq,double ppq,bool playing) {
    const bool play=rhythmRaw[0]->load()>.5f;
    if(play && !lastRhythmEnabled){rhythmMuted=false;tapeMuted=false;}
    lastRhythmEnabled=play;rhythm.enabled=play && !rhythmMuted;rhythm.followHost=rhythmRaw[1]->load()>.5f;
    rhythm.pattern=static_cast<int>(rhythmRaw[2]->load());rhythm.swing=rhythmRaw[3]->load();rhythm.volume=rhythmRaw[4]->load();rhythm.bpm=currentTempo.load();
    for(size_t t=0;t<5;++t) {
        rhythm.custom[t]=0;for(size_t s=0;s<16;++s)if(stepRaw[t][s]->load()>.5f)rhythm.custom[t]|=static_cast<std::uint16_t>(1u<<s);
        rhythm.samples[t]=nullptr;
        if(sampleLibrary) {
            int index=static_cast<int>(trackRaw[t]->load())-1;if(index<0)index=sampleLibrary->autoDrums[t];
            if(index>=0 && index<static_cast<int>(sampleLibrary->entries.size()))rhythm.samples[t]=&sampleLibrary->entries[static_cast<size_t>(index)].audio;
        }
    }
    sampleChords.sound=nullptr;
    if(sampleLibrary) {
        const int index=static_cast<int>(rhythmRaw[6]->load());
        if(index>=0 && index<static_cast<int>(sampleLibrary->entries.size()))sampleChords.sound=&sampleLibrary->entries[static_cast<size_t>(index)].audio;
    }
    sampleChords.rootNote=static_cast<int>(rhythmRaw[7]->load());sampleChords.release=rhythmRaw[8]->load()*.001f;
    sampleChords.wholeChord=sampleRaw[0]->load()>.5f;sampleChords.loop=sampleRaw[1]->load()>.5f;sampleChords.gain=raw[OrganGain]->load();
    sampleFilter.cutoff=sampleRaw[2]->load();sampleFilter.resonance=sampleRaw[3]->load();sampleFilter.mode=static_cast<int>(sampleRaw[4]->load());
    rhythm.beginBlock(hasPpq,ppq,playing);
}
