#include "SampleLibrary.h"
#include "FactorySamples.h"

std::unique_ptr<SampleLibrary> SampleLibrary::readZip(const juce::File& file,juce::String& error) {
    error.clear();
    if(!file.existsAsFile() || file.getSize()<22 || file.getSize()>256ll*1024*1024) {error="El ZIP no contiene audios o supera 256 MB.";return {};}
    juce::ZipFile zip(file);
    if(zip.getNumEntries()==0 || zip.getNumEntries()>2048) {error="ZIP vacio o no valido.";return {};}
    auto result=std::make_unique<SampleLibrary>();result->source=file.getFullPathName();
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::vector<int> indices;
    for(int i=0;i<zip.getNumEntries();++i) {
        const auto name=zip.getEntry(i)->filename;
        if(name.startsWith("__MACOSX/") || name.contains("/._") || name.startsWith("._"))continue;
        if(name.endsWithIgnoreCase(".wav") || name.endsWithIgnoreCase(".aif") || name.endsWithIgnoreCase(".aiff") || name.endsWithIgnoreCase(".flac"))indices.push_back(i);
    }
    std::sort(indices.begin(),indices.end(),[&](int a,int b){return zip.getEntry(a)->filename.compareNatural(zip.getEntry(b)->filename)<0;});
    if(indices.empty() || indices.size()>128) {error="El ZIP debe contener entre 1 y 128 samples WAV, AIFF o FLAC.";return {};}
    size_t totalFrames=0;
    for(const int i:indices) {
        const auto* entry=zip.getEntry(i);
        if(entry->uncompressedSize>64ll*1024*1024) {error="Un sample supera 64 MB.";return {};}
        std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(i));
        if(!stream) {error="No se ha podido leer "+entry->filename;return {};}
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
        if(!reader || reader->sampleRate<8000 || reader->sampleRate>192000 || reader->lengthInSamples<4 || reader->lengthInSamples>reader->sampleRate*30) {error="Audio no valido o de mas de 30 segundos: "+entry->filename;return {};}
        totalFrames+=static_cast<size_t>(reader->lengthInSamples);
        if(totalFrames>32u*1024u*1024u) {error="La libreria decodificada supera 256 MB.";return {};}
        const int count=static_cast<int>(reader->lengthInSamples);
        juce::AudioBuffer<float> buffer(2,count);
        if(!reader->read(&buffer,0,count,0,true,true)) {error="No se puede decodificar "+entry->filename;return {};}
        if(reader->numChannels==1)buffer.copyFrom(1,0,buffer,0,0,count);
        SampleEntry sound;sound.path=entry->filename.replace("\\","/");
        const auto leaf=sound.path.substring(sound.path.lastIndexOfChar('/')+1);
        sound.name=leaf.upToLastOccurrenceOf(".",false,false);sound.audio.rate=reader->sampleRate;
        const auto lower=sound.path.toLowerCase();
        if(lower.contains("drum loops/")) {
            // The supplied loops are four bars at 100 BPM. Other imported
            // loops may state a different tempo as their last filename token.
            const int writtenTempo=sound.name.getTrailingIntValue();
            const double bpm=writtenTempo>=20 && writtenTempo<=400?writtenTempo:100;
            sound.audio.beats=std::max(1.,std::round(count/reader->sampleRate*bpm/60));
        }
        const auto basename=sound.name.toLowerCase();
        sound.audio.drum=lower.contains("drum hits/") || basename.contains("kick") || basename.contains("snare")
            || basename.contains("hat") || basename.contains("cajon") || basename.contains("clave") || basename.contains("perc");
        for(const auto& info:chordharp::factorySamples)if(sound.name.equalsIgnoreCase(info.name)) {
            sound.audio.root=info.root;sound.audio.gain=info.gain;sound.audio.sustain=info.sustain;
        }
        for(size_t c=0;c<2;++c) {
            auto& data=sound.audio.channel[c];data.assign(buffer.getReadPointer(static_cast<int>(c)),buffer.getReadPointer(static_cast<int>(c))+count);
            for(auto& value:data)if(!std::isfinite(value)){error="El sample contiene audio no finito.";return {};}
        }
        result->entries.push_back(std::move(sound));
    }
    for(size_t i=0;i<result->entries.size();++i) {
        const auto& audio=result->entries[i].audio;
        if(audio.beats>0)result->loops.push_back(static_cast<int>(i));
        else if(!audio.drum)result->instruments.push_back(static_cast<int>(i));
    }
    for(int track=0;track<5;++track)result->autoDrums[static_cast<size_t>(track)]=result->drumForTrack(track);
    // This pack has no open hi-hat. Its auxiliary lane uses the real cajon;
    // the alternate snare remains available in the assignment menu.
    if(result->autoDrums[3]<0)for(size_t i=0;i<result->entries.size();++i)
        if(result->entries[i].name.containsIgnoreCase("cajon")){result->autoDrums[3]=static_cast<int>(i);break;}
    return result;
}
int SampleLibrary::drumForTrack(int track) const {
    static const juce::StringArray terms[]{ {"kick","bass drum","bd_","_bd","bombo"},{"snare","sd_","_sd","caja","clap"},{"closed","chh","hat","hh_"},{"open","ohh"},{"rim","perc","tom","clave","cowbell","conga","bongo"} };
    if(track<0 || track>=5)return -1;
    for(size_t i=0;i<entries.size();++i) {
        if(entries[i].audio.beats>0 || !entries[i].audio.drum)continue;
        const auto name=entries[i].name.toLowerCase();
        if(track==2 && (name.contains("open") || name.contains("ohh")))continue;
        for(const auto& term:terms[track])if(name.contains(term))return static_cast<int>(i);
    }
    return -1;
}
