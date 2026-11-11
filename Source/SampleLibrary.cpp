#include "SampleLibrary.h"
#include "FactorySamples.h"
#include <bit>
#include <map>
#include <mutex>

std::shared_ptr<const chordharp::GraySlateBank> SampleLibrary::graySlateBank(const juce::File& directory,bool harp,juce::String& error) {
    static std::mutex mutex;
    static std::map<juce::String,std::weak_ptr<const chordharp::GraySlateBank>> cache;
    const std::lock_guard<std::mutex> guard(mutex);
    const auto key=directory.getFullPathName()+(harp?":harp":":chords");
    if(auto shared=cache[key].lock())return shared;
    if(directory==juce::File{}){error="No se encuentra el banco Grayslate samples.";return {};}
    SampleLibrary catalog;if(!catalog.appendGraySlate(directory,error))return {};
    auto bank=std::make_shared<chordharp::GraySlateBank>();int count=0;
    const juce::StringArray roots{"c","db","d","eb","e","f","f#","g","ab","a","bb","b"};
    for(const auto& entry:catalog.entries) {
        auto group=entry.recording.getParentDirectory().getFileName();
        const bool supplement=!harp && !entry.wholeChord && entry.recording.getFileNameWithoutExtension()==group+"0" && !group.endsWith("m") && !group.endsWith("7");
        if(group=="rhythm" || (entry.wholeChord==harp && !supplement))continue;
        int quality=0;if(group.endsWith("m")){quality=1;group=group.dropLastCharacters(1);}else if(group.endsWith("7")){quality=2;group=group.dropLastCharacters(1);}
        const int root=roots.indexOf(group);if(root<0){error="Grupo de acorde GraySlate no valido.";return {};}
        chordharp::SampleAudio* destination=nullptr;
        if(supplement)destination=&bank->sevenths[static_cast<size_t>(root)];
        else if(!harp)destination=&bank->chords[static_cast<size_t>(quality*12+root)];
        else {
            const auto step=entry.recording.getFileNameWithoutExtension().substring(entry.recording.getParentDirectory().getFileName().length());
            const int index=step.getIntValue();if(step.isEmpty() || !step.containsOnly("0123456789") || index<0 || index>11){error="Posicion de arpa GraySlate no valida.";return {};}
            destination=&bank->harps[static_cast<size_t>(quality*12+root)][static_cast<size_t>(index)];
        }
        if(destination->frames()>0 || !decodeRecording(entry.recording,*destination,error,entry.archive))return {};
        destination->root=entry.audio.root;destination->gain=entry.audio.gain;destination->sustain=entry.audio.sustain;destination->compact(entry.bits==16);++count;
    }
    if(count!=(harp?431:48)){error="El banco GraySlate esta incompleto.";return {};}
    cache[key]=bank;error.clear();return bank;
}

juce::File SampleLibrary::bundledDirectory() {
    const auto resources=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getSiblingFile("Resources");
    return resources.getChildFile("grayslate-index.json").existsAsFile()?resources:juce::File{};
}
bool SampleLibrary::appendGraySlate(const juce::File& directory,juce::String& error) {
    if(directory==juce::File{})return true;
    const auto document=juce::JSON::parse(directory.getChildFile("grayslate-index.json").loadFileAsString());
    const auto* list=document["entries"].getArray();
    if(static_cast<int>(document["format"])!=1 || !list || list->isEmpty() || list->size()>1024){error="Catalogo GraySlate no valido.";return false;}
    std::vector<SampleEntry> added;
    const auto archive=directory.getChildFile("GraySlateSamples.zip");
    std::unique_ptr<juce::ZipFile> packed;if(archive.existsAsFile())packed=std::make_unique<juce::ZipFile>(archive);
    const auto folder=directory.getChildFile("GraySlate's Omnichord Pack");
    for(const auto& item:*list) {
        const auto path=item["file"].toString();
        if(path.isEmpty() || juce::File::isAbsolutePath(path) || path.contains("..") || path.contains("\\")){error="Ruta GraySlate no valida.";return false;}
        SampleEntry entry;entry.path="GraySlate/"+path;entry.name=item["name"].toString();entry.recording=folder.getChildFile(path);if(packed)entry.archive=archive;
        const int root=static_cast<int>(item["root"]);const float gain=static_cast<float>(item["gain"]);
        const bool available=packed?packed->getIndexOfFileName("GraySlate's Omnichord Pack/"+path)>=0:entry.recording.existsAsFile();
        if(!available || root<0 || root>127 || !std::isfinite(gain) || gain<=0 || gain>6){error="Falta una grabacion o sus metadatos no son validos: "+path;return false;}
        entry.audio.root=root;entry.audio.gain=gain;entry.audio.sustain=static_cast<bool>(item["loop"]);entry.wholeChord=static_cast<bool>(item["wholeChord"]);entry.bits=static_cast<int>(item["bits"]);
        added.push_back(std::move(entry));
    }
    for(auto& entry:added){instruments.push_back(static_cast<int>(entries.size()));entries.push_back(std::move(entry));}
    error.clear();return true;
}
bool SampleLibrary::decodeRecording(const juce::File& file,chordharp::SampleAudio& audio,juce::String& error,const juce::File& archive) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::ZipFile> packed;int entry=-1;
    if(archive.existsAsFile()) {
        packed=std::make_unique<juce::ZipFile>(archive);entry=packed->getIndexOfFileName(file.getRelativePathFrom(archive.getParentDirectory()).replace("\\","/"));
        if(entry<0 || packed->getEntry(entry)->uncompressedSize>64ll*1024*1024){error="Grabacion no encontrada en el banco.";return false;}
    }
    const auto stream=[&]() -> std::unique_ptr<juce::InputStream> {return packed?std::unique_ptr<juce::InputStream>(packed->createStreamForEntry(entry)):std::unique_ptr<juce::InputStream>(file.createInputStream());};
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(stream()));
    if(reader) {
        if(reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>192000 || reader->lengthInSamples<4 || reader->lengthInSamples>reader->sampleRate*30){error="Grabacion fuera de limites.";return false;}
        const int n=static_cast<int>(reader->lengthInSamples);juce::AudioBuffer<float> buffer(2,n);
        if(!reader->read(&buffer,0,n,0,true,true)){error="No se puede leer el WAV.";return false;}
        if(reader->numChannels==1)buffer.copyFrom(1,0,buffer,0,0,n);
        audio.rate=reader->sampleRate;
        for(size_t c=0;c<2;++c)audio.channel[c].assign(buffer.getReadPointer(static_cast<int>(c)),buffer.getReadPointer(static_cast<int>(c))+n);
    } else {
        // 103 originals use IEEE float64 WAV, unsupported by JUCE's reader.
        // Convert to the engine's float buffers on load, retaining the 96 kHz rate.
        juce::MemoryBlock block;
        auto input=stream();
        if(!input || input->getTotalLength()>64ll*1024*1024 || input->readIntoMemoryBlock(block,64ll*1024*1024)<12){error="WAV no valido.";return false;}
        const auto* bytes=static_cast<const unsigned char*>(block.getData());const auto size=block.getSize();
        if(std::memcmp(bytes,"RIFF",4)!=0 || std::memcmp(bytes+8,"WAVE",4)!=0){error="Formato WAV no valido.";return false;}
        unsigned channels=0,rate=0,bits=0,code=0;size_t start=0,length=0;
        for(size_t offset=12;offset+8<=size;) {
            const size_t count=juce::ByteOrder::littleEndianInt(bytes+offset+4);offset+=8;if(count>size-offset){error="WAV truncado.";return false;}
            if(std::memcmp(bytes+offset-8,"fmt ",4)==0 && count>=16){code=juce::ByteOrder::littleEndianShort(bytes+offset);channels=juce::ByteOrder::littleEndianShort(bytes+offset+2);rate=juce::ByteOrder::littleEndianInt(bytes+offset+4);bits=juce::ByteOrder::littleEndianShort(bytes+offset+14);}
            if(std::memcmp(bytes+offset-8,"data",4)==0){start=offset;length=count;}
            offset+=count+count%2;
        }
        if(code!=3 || bits!=64 || channels<1 || channels>2 || rate<8000 || rate>192000 || length<channels*8*4 || length%(channels*8)!=0 || length/(channels*8)>rate*30){error="WAV float64 no valido.";return false;}
        const size_t frames=length/(channels*8);audio.rate=rate;
        for(size_t c=0;c<2;++c){auto& output=audio.channel[c];output.resize(frames);for(size_t n=0;n<frames;++n)output[n]=static_cast<float>(std::bit_cast<double>(juce::ByteOrder::littleEndianInt64(bytes+start+(n*channels+std::min(c,static_cast<size_t>(channels-1)))*8)));}
    }
    for(const auto& channel:audio.channel)for(float x:channel)if(!std::isfinite(x)){error="El WAV contiene valores no finitos.";return false;}
    error.clear();return true;
}

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
        sound.audio.openHat=basename.contains("open") || basename.contains("ohh");
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
