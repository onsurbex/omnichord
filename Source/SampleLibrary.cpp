#include "SampleLibrary.h"

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
        SampleEntry sound;sound.name=entry->filename;sound.audio.rate=reader->sampleRate;
        for(size_t c=0;c<2;++c) {
            auto& data=sound.audio.channel[c];data.assign(buffer.getReadPointer(static_cast<int>(c)),buffer.getReadPointer(static_cast<int>(c))+count);
            for(auto& value:data)if(!std::isfinite(value)){error="El sample contiene audio no finito.";return {};}
        }
        result->entries.push_back(std::move(sound));
    }
    for(int track=0;track<5;++track)result->autoDrums[static_cast<size_t>(track)]=result->drumForTrack(track);
    return result;
}
int SampleLibrary::drumForTrack(int track) const {
    static const juce::StringArray terms[]{ {"kick","bass drum","bd_","_bd","bombo"},{"snare","sd_","_sd","caja","clap"},{"closed","chh","hat","hh_"},{"open","ohh"},{"rim","perc","tom","clave","cowbell","conga","bongo"} };
    if(track<0 || track>=5)return -1;
    for(size_t i=0;i<entries.size();++i) {
        const auto name=entries[i].name.toLowerCase();
        if(track==2 && (name.contains("open") || name.contains("ohh")))continue;
        for(const auto& term:terms[track])if(name.contains(term))return static_cast<int>(i);
    }
    return -1;
}
