#include "PluginProcessor.h"

void ChordHarpProcessor::timerCallback() {
    juce::String error;ensureSampleLoaded(static_cast<int>(rhythmRaw[6]->load()),error);
}
bool ChordHarpProcessor::ensureSampleLoaded(int index,juce::String& error) {
    const juce::ScopedLock loadLock(sampleLoadLock);juce::File file,archive;
    {
        const juce::ScopedLock lock(libraryLock);
        if(!sampleLibrary || index<0 || index>=static_cast<int>(sampleLibrary->entries.size()))return true;
        const auto& entry=sampleLibrary->entries[static_cast<size_t>(index)];
        if(entry.recording==juce::File{} || entry.audio.frames()>0)return true;
        file=entry.recording;archive=entry.archive;
    }
    chordharp::SampleAudio decoded;
    if(!SampleLibrary::decodeRecording(file,decoded,error,archive))return false;
    // All file I/O and decoding happen outside the audio callback and its lock.
    const juce::ScopedLock audioLock(getCallbackLock());const juce::ScopedLock lock(libraryLock);
    if(!sampleLibrary || index>=static_cast<int>(sampleLibrary->entries.size()) || sampleLibrary->entries[static_cast<size_t>(index)].recording!=file)return false;
    sampleChords.reset();sampleChords.sound=nullptr;
    for(auto& entry:sampleLibrary->entries)if(entry.recording!=juce::File{})entry.audio.channel={};
    auto& target=sampleLibrary->entries[static_cast<size_t>(index)].audio;
    decoded.root=target.root;decoded.gain=target.gain;decoded.sustain=target.sustain;target=std::move(decoded);
    return true;
}

juce::String ChordHarpProcessor::rhythmStepID(int track,int step) {return "rhythmStep"+juce::String(track)+"_"+juce::String(step);}
int ChordHarpProcessor::rhythmPatternLength() const {
    return std::min(juce::jlimit(1,16,static_cast<int>(rhythmRaw[11]->load())),static_cast<int>(rhythmRaw[2]->load())==5?12:16);
}
void ChordHarpProcessor::makeRhythmCustom() {
    const int pattern=static_cast<int>(rhythmRaw[2]->load());if(pattern>=6)return;
    const int length=rhythmPatternLength();
    const auto set=[&](const juce::String& id,float v){auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));};
    for(int t=0;t<5;++t)for(int step=0;step<16;++step)set(rhythmStepID(t,step),(chordharp::rhythmPatterns[static_cast<size_t>(pattern)][static_cast<size_t>(t)]&(1u<<step))!=0?1.f:0.f);
    set("rhythmLength",static_cast<float>(length));set("rhythmPattern",6);
}
void ChordHarpProcessor::resizeRhythmPattern(int length) {
    length=juce::jlimit(1,16,length);if(length==rhythmPatternLength())return;
    makeRhythmCustom();auto* p=state.getParameter("rhythmLength");p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(length)));
}
void ChordHarpProcessor::editRhythmStep(int track,int step,bool on) {
    if(track<0 || track>=5 || step<0 || step>=rhythmPatternLength())return;
    makeRhythmCustom();auto* p=state.getParameter(rhythmStepID(track,step));p->beginChangeGesture();p->setValueNotifyingHost(on?1.f:0.f);p->endChangeGesture();
}
juce::StringArray ChordHarpProcessor::sampleNames() const {
    const juce::ScopedLock lock(libraryLock);juce::StringArray names;
    if(sampleLibrary)for(const auto& sound:sampleLibrary->entries)names.add(sound.name);return names;
}
juce::String ChordHarpProcessor::sampleLibraryPath() const {const juce::ScopedLock lock(libraryLock);return sampleLibrary?sampleLibrary->source:juce::String{};}
juce::String ChordHarpProcessor::sampleLibraryReference() const {
    const auto path=sampleLibraryPath();return path.isNotEmpty() && path==factoryLibraryPath?juce::String("factory"):path;
}
bool ChordHarpProcessor::restoreSampleLibrary(const juce::String& reference,juce::String& error) {
    const auto path=reference=="factory"?factoryLibraryPath:reference;
    if(path.isEmpty()){error="La libreria de fabrica no esta disponible.";return false;}
    if(path==sampleLibraryPath()){error.clear();return true;}
    return loadSampleLibrary(juce::File(path),error);
}
std::vector<std::pair<int,juce::String>> ChordHarpProcessor::sampleChoices(bool loops) const {
    const juce::ScopedLock lock(libraryLock);std::vector<std::pair<int,juce::String>> result;
    if(sampleLibrary)for(const auto index:loops?sampleLibrary->loops:sampleLibrary->instruments)
        result.emplace_back(index,sampleLibrary->entries[static_cast<size_t>(index)].name);
    return result;
}
std::vector<std::pair<int,juce::String>> ChordHarpProcessor::drumChoices() const {
    const juce::ScopedLock lock(libraryLock);std::vector<std::pair<int,juce::String>> result;
    if(sampleLibrary) {
        for(size_t i=0;i<sampleLibrary->entries.size();++i)if(sampleLibrary->entries[i].audio.drum)
            result.emplace_back(static_cast<int>(i),sampleLibrary->entries[i].name);
        if(result.empty())for(const int i:sampleLibrary->instruments)result.emplace_back(i,sampleLibrary->entries[static_cast<size_t>(i)].name);
    }
    return result;
}
void ChordHarpProcessor::selectSampleSound(int index,bool activate) {
    juce::String error;if(!ensureSampleLoaded(index,error))return;
    int root=60;bool loop=false,whole=false;
    {
        const juce::ScopedLock lock(libraryLock);
        if(!sampleLibrary || index<0 || index>=static_cast<int>(sampleLibrary->entries.size()))return;
        const auto& entry=sampleLibrary->entries[static_cast<size_t>(index)];const auto& audio=entry.audio;root=audio.root;loop=audio.sustain;whole=entry.wholeChord;
    }
    const auto set=[&](const char* id,float value){auto* p=state.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();};
    set("organSample",static_cast<float>(index));set("sampleRoot",static_cast<float>(root));
    set("sampleChord",whole?1.f:0.f);set("sampleLoop",loop?1.f:0.f);set("sampleRelease",loop?800.f:250.f);
    if(activate)set("samplesEnabled",1);
}
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
    if(cache.getFullPathName()==factoryLibraryPath && !decoded->appendGraySlate(factoryCatalogRoot,error))return false;
    {
        // Decode and copy outside the callback lock. Swap only while processing
        // is excluded by JUCE's host/standalone wrappers; no file I/O on audio.
        const juce::ScopedLock audioLock(getCallbackLock());
        const juce::ScopedLock metadataLock(libraryLock);
        sampleChords.reset();rhythm.reset();sampleFilter.reset();sampleDC.reset();
        rhythm.samples={};rhythm.loopSound=nullptr;sampleChords.sound=nullptr;
        sampleLibrary.swap(decoded);++sampleRevision;
    }
    // Indices from the previous ZIP have no meaning in the new library.
    // A preset/session restores its saved assignments after loading the ZIP.
    const auto set=[&](const juce::String& id,float value) {auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));};
    if(!sampleLibrary->instruments.empty())selectSampleSound(sampleLibrary->instruments[0],false);
    else set("organSample",0);
    set("rhythmLoop",sampleLibrary->loops.empty()?0.f:static_cast<float>(sampleLibrary->loops[0]));
    for(int track=0;track<5;++track)set("rhythmSample"+juce::String(track),0);
    error.clear();return true;
}
void ChordHarpProcessor::readRhythmParameters(bool hasPpq,double ppq,bool playing,bool hasTransport) {
    const bool play=rhythmRaw[0]->load()>.5f;
    const bool hostRun=hasTransport && playing && rhythmRaw[1]->load()>.5f;
    if(play && (!lastRhythmEnabled || (hostRun && !lastRhythmHostPlay))){rhythmMuted=false;tapeMuted=false;}
    lastRhythmHostPlay=hostRun;
    lastRhythmEnabled=play;rhythm.enabled=play && !rhythmMuted;rhythm.followHost=rhythmRaw[1]->load()>.5f;
    rhythm.useLoop=rhythmRaw[9]->load()>.5f;rhythm.loopSound=nullptr;
    if(sampleLibrary) {
        const int index=static_cast<int>(rhythmRaw[10]->load());
        if(index>=0 && index<static_cast<int>(sampleLibrary->entries.size()) && sampleLibrary->entries[static_cast<size_t>(index)].audio.beats>0)
            rhythm.loopSound=&sampleLibrary->entries[static_cast<size_t>(index)].audio;
    }
    rhythm.pattern=static_cast<int>(rhythmRaw[2]->load());rhythm.length=static_cast<int>(rhythmRaw[11]->load());rhythm.swing=rhythmRaw[3]->load();rhythm.volume=1;rhythm.bpm=currentTempo.load();
    for(size_t t=0;t<5;++t) {
        rhythm.custom[t]=0;for(size_t s=0;s<16;++s)if(stepRaw[t][s]->load()>.5f)rhythm.custom[t]|=static_cast<std::uint16_t>(1u<<s);
        rhythm.samples[t]=nullptr;
        if(sampleLibrary) {
            int index=static_cast<int>(trackRaw[t]->load())-1;if(index<0)index=sampleLibrary->autoDrums[t];
            if(index>=0 && index<static_cast<int>(sampleLibrary->entries.size()))rhythm.samples[t]=&sampleLibrary->entries[static_cast<size_t>(index)].audio;
        }
    }
    rhythm.chokeOpenHat=rhythm.samples[3] && rhythm.samples[3]->openHat;
    sampleChords.sound=nullptr;
    if(sampleLibrary) {
        const int index=static_cast<int>(rhythmRaw[6]->load());
        if(index>=0 && index<static_cast<int>(sampleLibrary->entries.size())) {
            const auto& audio=sampleLibrary->entries[static_cast<size_t>(index)].audio;
            if(!audio.drum && audio.beats==0)sampleChords.sound=&audio;
        }
    }
    sampleChords.rootNote=static_cast<int>(rhythmRaw[7]->load());sampleChords.release=rhythmRaw[8]->load()*.001f;
    sampleChords.wholeChord=sampleRaw[0]->load()>.5f;sampleChords.loop=sampleRaw[1]->load()>.5f;
    sampleChords.gain=sampleChords.sound?sampleChords.sound->gain:1.f;
    sampleFilter.cutoff=sampleFilterRefs[0]->load();sampleFilter.resonance=sampleFilterRefs[1]->load();sampleFilter.mode=static_cast<int>(sampleFilterRefs[2]->load());
    rhythm.beginBlock(hasPpq,ppq,playing,hasTransport);
}
