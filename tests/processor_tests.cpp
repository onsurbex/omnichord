#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "BinaryData.h"
#include "../Source/Themes.h"
#include "AmbienceChecks.h"
#include "../Source/FMEditor.h"
#include "../Source/FactorySamples.h"
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
#include <foleys_gui_magic/Editor/foleys_ToolBox.h>
#endif
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
static bool backgroundPassed=true;
static void waitForGui(const std::function<bool()>& ready,const char* message) {
    for(int attempt=0;attempt<40 && !ready();++attempt)juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    require(ready(),message);
}

static juce::TabbedButtonBar* findTabBar(juce::Component& parent) {
    // The main bar is a sibling of its pages; a depth-first walk reaches the
    // nested FX bar first. Search by depth to find the requested outer group.
    std::vector<juce::Component*> pending{&parent};
    for(size_t i=0;i<pending.size();++i) {
        if(auto* tabs=dynamic_cast<juce::TabbedButtonBar*>(pending[i]))return tabs;
        for(int n=0;n<pending[i]->getNumChildComponents();++n)pending.push_back(pending[i]->getChildComponent(n));
    }
    return nullptr;
}

static juce::Component* findComponent(juce::Component& parent,const juce::String& id) {
    if(parent.getComponentID()==id) return &parent;
    for(int i=0;i<parent.getNumChildComponents();++i) if(auto* item=findComponent(*parent.getChildComponent(i),id)) return item;
    return nullptr;
}

static void checkLogoThemes(ChordHarpProcessor& processor,ChordHarpEditor& editor) {
    auto* logo=editor.getGUIBuilder().findGuiItemWithId("logo")->getWrappedComponent();
    const auto assertLogo=[&](bool white) {
        const auto snapshot=logo->createComponentSnapshot(logo->getLocalBounds());
        require(snapshot.getPixelAt(0,0).getAlpha()==0,"Logo must have a transparent background without a cream plate");
        double brightness=0; int pixels=0;
        for(int y=0;y<snapshot.getHeight();++y) for(int x=0;x<snapshot.getWidth();++x) {
            const auto c=snapshot.getPixelAt(x,y);
            if(c.getAlpha()>220) { brightness+=c.getBrightness(); ++pixels; }
        }
        require(pixels>100,"The chosen logo must contain visible artwork");
        if(!(white?brightness/pixels>0.8:brightness/pixels<0.2))std::cerr<<"Logo expected white="<<white<<" mean="<<brightness/pixels<<" theme="<<processor.state.getRawParameterValue("theme")->load()<<" background="<<omnichord::logoBackground(processor.magicState.getGuiTree(),static_cast<int>(processor.state.getRawParameterValue("theme")->load())).toString()<<'\n';
        require(white?brightness/pixels>0.8:brightness/pixels<0.2,"Rendered logo must match the contrast of the panel");
    };
    auto* parameter=processor.state.getParameter("theme");
    for(int theme=0;theme<6;++theme) {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(theme)));
        for(int attempt=0;attempt<20;++attempt) {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            if(omnichord::logoBackground(processor.magicState.getGuiTree(),theme)==juce::Colour(omnichord::themes[static_cast<size_t>(theme)].panel))break;
        }
        require(omnichord::logoBackground(processor.magicState.getGuiTree(),theme)==juce::Colour(omnichord::themes[static_cast<size_t>(theme)].panel),"Each selected theme must reach the actual panel palette");
        assertLogo(theme==2);
        const auto full=editor.createComponentSnapshot(editor.getLocalBounds());
        if(full.getPixelAt(0,0)!=juce::Colour(0xff292a2d) || full.getPixelAt(full.getWidth()-1,full.getHeight()-1)!=juce::Colour(0xff292a2d)) {
            backgroundPassed=false;
            std::cerr << "Theme " << theme << " snapshot " << full.getWidth() << 'x' << full.getHeight() << " editor " << editor.getBounds().toString()
                      << " opaque=" << editor.isOpaque() << " outer corners: " << full.getPixelAt(0,0).toString() << ", " << full.getPixelAt(full.getWidth()-1,full.getHeight()-1).toString() << '\n';
        }
        for(const auto* id:{"pad","harpQuantize","chordSustain","tapeEnabled"})
            require(editor.getGUIBuilder().findGuiItemWithId(id)!=nullptr,"All physical switches must survive theme changes");
        if(theme==1) {
            require(omnichord::themeNames()[1]=="Pinky","Pinky must replace the Crema preset without changing its index");
            require(omnichord::logoBackground(processor.magicState.getGuiTree(),1)==juce::Colour(0xfff0d6e9),"Pinky must use the user's exact panel colour");
        }
    }
    parameter->setValueNotifyingHost(0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    auto palette=processor.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
    const auto original=palette.getProperty("color-ffeee6cf");
    palette.setProperty("color-ffeee6cf","ff202020",nullptr);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100); assertLogo(true);
    palette.setProperty("color-ffeee6cf",original,nullptr);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100); assertLogo(false);
}

static void checkChordReleaseAndTape() {
    ChordHarpProcessor piano,reference;
    for(auto* synth:{&piano,&reference}) {
        synth->state.getParameter("pad")->setValueNotifyingHost(1);
        synth->state.getParameter("organEnabled")->setValueNotifyingHost(1);
        synth->state.getParameter("chordSustain")->setValueNotifyingHost(0);
        synth->prepareToPlay(48000,512);
    }
    juce::AudioBuffer<float> audio(2,512),control(2,512); juce::MidiBuffer midi,other;
    midi.addEvent(juce::MidiMessage::noteOn(1,24,0.8f),0);
    other.addEvent(juce::MidiMessage::noteOn(1,24,0.8f),0);
    for(int b=0;b<20;++b) { piano.processBlock(audio,midi); reference.processBlock(control,other); }
    require(audio.getMagnitude(0,512)>0.001f,"Unlatched chord must sound while MIDI is held");
    midi.addEvent(juce::MidiMessage::noteOff(1,24),128);
    piano.processBlock(audio,midi); reference.processBlock(control,other);
    for(int i=0;i<128;++i) require(audio.getSample(0,i)==control.getSample(0,i),"Note Off must respect its exact MIDI sample offset");
    require(audio.getMagnitude(0,128,384)>0.0001f,"Note Off must produce a release envelope rather than a hard cut");
    for(int b=0;b<120;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)<0.00001f,"Unlatched chord must finish its release after the final Note Off");
    midi.addEvent(juce::MidiMessage::noteOn(1,24,0.8f),0);
    midi.addEvent(juce::MidiMessage::noteOn(2,25,0.8f),256);
    piano.processBlock(audio,midi); require(piano.selectedChord()==8,"Newest chord has priority across MIDI channels");
    midi.addEvent(juce::MidiMessage::noteOff(2,25),0); piano.processBlock(audio,midi);
    require(piano.selectedChord()==1,"Releasing the newest MIDI chord returns to the older held chord");
    for(int b=0;b<20;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"The older held chord must remain audible");
    piano.state.getParameter("chordSustain")->setValueNotifyingHost(1);
    midi.addEvent(juce::MidiMessage::noteOff(1,24),0);
    for(int b=0;b<120;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"Sustain ON keeps the chord after Note Off");
    piano.state.getParameter("chordSustain")->setValueNotifyingHost(0);
    for(int b=0;b<120;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)<0.00001f,"Turning sustain OFF with no held keys releases the chord");
    piano.pressChord(37,true);
    for(int b=0;b<20;++b) piano.processBlock(audio,midi);
    require(piano.selectedChord()==37 && audio.getMagnitude(0,512)>0.001f,"Mac Maj7 key-down must open the chord gate");
    piano.releaseChord(37,true);
    for(int b=0;b<120;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)<0.00001f,"Mac key-up must release the chord gate");
    piano.state.getParameter("tapeEnabled")->setValueNotifyingHost(1);
    piano.state.getParameter("tapeNoise")->setValueNotifyingHost(1);
    for(int b=0;b<40;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.0001f,"Tape noise must reach the processor output");
    midi.addEvent(juce::MidiMessage::allSoundOff(1),0); piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)==0,"Panic must clear tape history and mute hiss");
    midi.addEvent(juce::MidiMessage::noteOn(1,72,0.8f),0);
    for(int b=0;b<4;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.0001f,"A new performance resumes tape after panic");
    piano.state.getParameter("gain")->setValueNotifyingHost(0);
    for(int b=0;b<10;++b) piano.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)==0,"Master volume must also mute tape noise");
}

static void checkPresetLibrary() {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Omnichord preset checks","",true);
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup{root};
    ChordHarpProcessor source(root);
    std::vector<float> expected;
    for(auto* parameter:source.getParameters()) {
        // JUCE bools retain fractional host values internally, but their legal
        // states (and the preset's engineering values) are OFF=0 and ON=1.
        parameter->setValueNotifyingHost(parameter->isBoolean()?1.0f:.73f);expected.push_back(parameter->getValue());
    }
    juce::String error; const auto file=source.getPresetDirectory().getChildFile("Mi preset completo.omnipreset");
    require(source.savePreset(file,error),"Saving a full preset must succeed");
    for(auto* parameter:source.getParameters()) parameter->setValueNotifyingHost(0);
    require(source.loadPreset(file,error),"Saved presets must reload");
    for(int i=0;i<source.getParameters().size();++i) {
        if(std::abs(source.getParameters()[i]->getValue()-expected[static_cast<size_t>(i)])>=.001f)
            std::cerr << "Preset mismatch: " << source.getParameters()[i]->getName(100) << " expected " << expected[static_cast<size_t>(i)] << " actual " << source.getParameters()[i]->getValue() << '\n';
        require(std::abs(source.getParameters()[i]->getValue()-expected[static_cast<size_t>(i)])<.001f,"Every plugin parameter must round-trip through a preset");
    }
    const auto collision=source.getPresetDirectory().getChildFile("Inicio.omnipreset");
    require(source.savePreset(collision,error) && source.loadFactoryPreset(0,error),"Factory and user presets may share a name");
    require(source.getPresetSource()=="factory:0","The factory preset keeps its identity despite a matching user filename");
    require(source.loadPreset(file,error),"Restore the test snapshot before choosing its default");
    const auto layout=source.magicState.getGuiTree().toXmlString();
    require(source.setStartupPreset(error),"A complete snapshot can be chosen as startup preset");
    ChordHarpProcessor next(root);
    for(int i=0;i<next.getParameters().size();++i)
        require(std::abs(next.getParameters()[i]->getValue()-expected[static_cast<size_t>(i)])<.001f,"A new instance must start from all chosen default parameters");
    const auto bad=root.getChildFile("corrupt.omnipreset");bad.replaceWithText("<OmnichordPreset formatVersion=\"2\"><PARAM id=\"gain\" value=\"0.1\"/><PARAM id=\"tone\" value=\"nan\"/></OmnichordPreset>");
    require(!next.loadPreset(bad,error),"Invalid preset values must be rejected");
    require(std::abs(next.getParameters()[0]->getValue()-expected[0])<.001f,"A corrupt preset must not partially change the sound");
    for(auto* parameter:source.getParameters()) parameter->setValueNotifyingHost(0);
    require(source.savePreset(root.getChildFile("minimum.omnipreset"),error) && source.loadPreset(root.getChildFile("minimum.omnipreset"),error),"Parameter minima must survive preset encoding");
    juce::MemoryBlock project;source.getStateInformation(project);next.setStateInformation(project.getData(),static_cast<int>(project.getSize()));
    require(next.state.getRawParameterValue("gain")->load()==0,"DAW project state must override the startup preset");
    for(int i=0;i<ChordHarpProcessor::factoryPresetNames().size();++i) require(next.loadFactoryPreset(i,error),"Every factory preset must load");
    require(source.magicState.getGuiTree().toXmlString()==layout,"Loading sound presets must preserve the edited panel layout");
    require(source.clearStartupPreset(error),"The default can be returned to factory settings");
    ChordHarpProcessor factory(root);require(factory.state.getRawParameterValue("organModel")->load()==0 && factory.state.getRawParameterValue("organEnabled")->load()==0,"Factory startup must be deterministic and silent");
}
static void checkFMPerformance() {
    ChordHarpProcessor synth;
    synth.selectSynthSound(false,4); // FM Pad: audible sustain until note-off.
    for(int op=1;op<=6;++op) {
        auto* p=synth.state.getParameter("harpOp"+juce::String(op)+"Release");p->setValueNotifyingHost(p->convertTo0to1(20));
    }
    synth.state.getParameter("sustain")->setValueNotifyingHost(synth.state.getParameter("sustain")->convertTo0to1(.8f));
    synth.prepareToPlay(48000,512);require(synth.getLatencySamples()==16,"The FM FIR latency must be reported to the host");
    juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(3,72,.9f),0);
    for(int b=0;b<70;++b)synth.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>.0001f,"A MIDI FM note must sustain while held");
    synth.selectChord(19);synth.processBlock(audio,midi);
    midi.addEvent(juce::MidiMessage::noteOff(3,72),128);
    for(int b=0;b<70;++b)synth.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)<.00001f,"FM note-off must release its original MIDI voice after the selected chord changes");
    synth.selectSynthSound(true,7);synth.selectSynthSound(false,7);
    synth.state.getParameter("organOp6Fine")->setValueNotifyingHost(.8f);
    juce::MemoryBlock state;synth.getStateInformation(state);
    auto restoredOwner=std::make_unique<ChordHarpProcessor>();auto& restored=*restoredOwner;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    for(const auto* id:{"organOp6Fine","organAlgorithm","harpAlgorithm","harpOp4Ratio","organOp2Sustain"})
        require(synth.state.getRawParameterValue(id)->load()==restored.state.getRawParameterValue(id)->load(),"FM bank settings must survive project recall");
}
static void checkSamplesAndRhythm() {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Omnichord sample checks","",true);
    struct Cleanup {juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    require(root.createDirectory().wasOk(),"The test sample folder must exist");
    juce::MemoryBlock wav;
    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::MemoryOutputStream(wav,false),48000,1,24,{},0));
        require(writer!=nullptr,"The audio fixture writer must open");
        juce::AudioBuffer<float> data(1,24000);
        for(int i=0;i<data.getNumSamples();++i)data.setSample(0,i,.4f*std::sin(static_cast<float>(chordharp::tau*220*i/48000)));
        require(writer->writeFromAudioSampleBuffer(data,0,data.getNumSamples()),"The audio fixture must encode");
    }
    const auto file=root.getChildFile("fixture.zip");
    {
        juce::ZipFile::Builder builder;
        for(const auto* name:{"Kick.wav","Snare.wav","Closed Hat.wav","Open Hat.wav","Perc.wav","Omni.wav"})builder.addEntry(new juce::MemoryInputStream(wav,true),6,name,juce::Time::getCurrentTime());
        auto output=file.createOutputStream();require(output && builder.writeToStream(*output,nullptr),"The sample ZIP fixture must encode");
    }
    ChordHarpProcessor synth(root);juce::String error;
    const auto invalid=root.getChildFile("empty.zip");invalid.replaceWithText("\r\n");
    require(!synth.loadSampleLibrary(invalid,error),"A two-byte placeholder must be rejected as a sample library");
    require(synth.loadSampleLibrary(file,error) && synth.sampleNames().size()==6,"A valid ZIP must decode all samples");
    const auto importedNames=synth.sampleNames();const int selectedSample=static_cast<int>(synth.state.getRawParameterValue("organSample")->load());
    require(selectedSample>=0 && selectedSample<importedNames.size() && importedNames[selectedSample]=="Omni" && synth.state.getRawParameterValue("sampleRoot")->load()==60,
        "Replacing the larger factory pack must select the new instrument and its root instead of keeping an invalid index");
    require(juce::File(synth.sampleLibraryPath()).existsAsFile() && synth.sampleLibraryPath()!=file.getFullPathName(),"Imported samples must be cached for session recall");
    const auto set=[&](const char* id,float value){auto* p=synth.state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));};
    set("samplesEnabled",1);set("pad",1);set("chordSustain",0);set("sampleLoop",1);
    synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,24,.8f),0);
    for(int i=0;i<70;++i)synth.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>.001f,"The chord sampler must loop a loaded sample while held");
    midi.addEvent(juce::MidiMessage::noteOff(1,24),0);
    for(int i=0;i<100;++i)synth.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)<.00001f,"The chord sampler must release on MIDI note-off");
    set("pad",0);set("rhythmEnabled",1);
    double energy=0;for(int b=0;b<100;++b){synth.processBlock(audio,midi);energy+=audio.getMagnitude(0,512);}
    require(energy>.01,"The rhythm engine must play the imported drum samples");
    synth.editRhythmStep(0,2,true);
    require(synth.state.getRawParameterValue("rhythmPattern")->load()==6 && synth.state.getRawParameterValue("rhythmStep0_2")->load()==1
        && synth.state.getRawParameterValue("rhythmStep1_4")->load()==1,"Editing a step must preserve the other steps in the selected pattern");
    juce::MemoryBlock state;synth.getStateInformation(state);
    ChordHarpProcessor restored(root);restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    require(restored.sampleNames().size()==6 && restored.state.getRawParameterValue("rhythmStep0_2")->load()==1,"Sessions must recover the sample library and custom rhythm");
    const auto preset=root.getChildFile("Samples.omnipreset");require(synth.savePreset(preset,error),"A sample preset must save");
    ChordHarpProcessor presetRestored(root);require(presetRestored.loadPreset(preset,error) && presetRestored.sampleNames().size()==6,"Presets must recover the cached library");
    require(!synth.loadSampleLibrary(invalid,error) && synth.sampleNames().size()==6,"An invalid import must preserve the loaded library");
    synth.panic.store(true);synth.processBlock(audio,midi);require(audio.getMagnitude(0,512)==0,"Panic must clear sample voices and drum playback");
}
static void checkFactorySamples() {
#if OMNICHORD_HAS_FACTORY_SAMPLES
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Omnichord factory audio checks","",true);
    struct Cleanup {juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    ChordHarpProcessor synth(root);juce::String error;
    require(synth.sampleNames().size()==15 && synth.sampleChoices().size()==5 && synth.sampleChoices(true).size()==4,"The supplied factory ZIP must bundle 5 instruments, 6 drum hits and 4 loops");
    auto decoded=SampleLibrary::readZip(juce::File(synth.sampleLibraryPath()),error);
    require(decoded && decoded->instruments.size()==5 && decoded->loops.size()==4,"The original recordings must decode from the installed factory library");
    for(const int i:decoded->loops)require(decoded->entries[static_cast<size_t>(i)].audio.beats==16,"100 BPM loops must occupy exactly four bars");
    for(const int i:decoded->autoDrums)require(i>=0,"Every sequencer lane must have a real factory drum assignment");
    for(int i=0;i<5;++i) {
        require(synth.loadFactoryPreset(12+i,error),"Each sampled factory sound must load");
        require(static_cast<int>(synth.state.getRawParameterValue("sampleRoot")->load())==chordharp::factorySamples[static_cast<size_t>(i)].root,"Sample presets must select the measured MIDI root, including F bass and upper C recordings");
        require(synth.state.getRawParameterValue("samplesEnabled")->load()==1,"A sample preset must activate the chord sampler");
        synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1,29,.9f),0);double energy=0;
        for(int b=0;b<30;++b){synth.processBlock(audio,midi);energy+=audio.getMagnitude(0,512);}
        require(energy>.01,"Every supplied instrument must sound when played as a chord");
    }
    for(int preset=17;preset<23;++preset) {
        require(synth.loadFactoryPreset(preset,error),"Each recorded or sequenced rhythm preset must load");
        synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;double energy=0;
        for(int b=0;b<30;++b){synth.processBlock(audio,midi);energy+=audio.getMagnitude(0,512);}
        require(energy>.01,"A playing rhythm preset must start after clearing the previous preset's voices");
    }
    const auto saved=root.getChildFile("Factory samples.omnipreset");
    require(synth.savePreset(saved,error),"A preset using the factory pack must save");
    const auto xml=juce::ValueTree::fromXml(saved.loadFileAsString());
    require(xml.getProperty("sampleLibrary").toString()=="factory","Bundled sample references must be portable between user accounts");
    ChordHarpProcessor other(root.getChildFile("another-account"));
    require(other.loadPreset(saved,error) && other.sampleNames().size()==15,"A factory sample preset must load from a different cache directory");
#endif
}
class TestPlayhead final : public juce::AudioPlayHead {
public:
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo position;position.setBpm(90);return position; }
};
static void checkTempoAndAmbience() {
    ChordHarpProcessor synth; synth.state.getParameter("delayTempo")->setValueNotifyingHost(synth.state.getParameter("delayTempo")->convertTo0to1(150));
    synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;synth.processBlock(audio,midi);
    require(std::abs(synth.getCurrentTempo()-150)<.01 && !synth.tempoComesFromHost(),"Standalone must use the editable BPM");
    TestPlayhead playhead;synth.setPlayHead(&playhead);synth.processBlock(audio,midi);
    require(synth.getCurrentTempo()==90 && synth.tempoComesFromHost(),"Host BPM must override standalone tempo");
    synth.setPlayHead(nullptr);
    checkAmbienceDSP(require);
}

static void checkEffectBypass() {
    for(const juce::String effect:{"delay","reverb"}) {
        ChordHarpProcessor bypass,dry;
        bypass.state.getParameter(effect+"Mix")->setValueNotifyingHost(.8f);
        bypass.state.getParameter(effect=="delay"?"harpDelaySend":"harpReverbSend")->setValueNotifyingHost(.8f);
        bypass.state.getParameter(effect+"Enabled")->setValueNotifyingHost(0);
        for(auto* synth:{&bypass,&dry}) {
            auto* time=synth->state.getParameter("delayTime");time->setValueNotifyingHost(time->convertTo0to1(80));
            synth->prepareToPlay(48000,512);
        }
        juce::AudioBuffer<float> a(2,512),b(2,512);juce::MidiBuffer ma,mb;
        ma.addEvent(juce::MidiMessage::noteOn(1,72,.8f),0);mb.addEvent(juce::MidiMessage::noteOn(1,72,.8f),0);
        for(int block=0;block<60;++block) {
            bypass.processBlock(a,ma);dry.processBlock(b,mb);
            for(int c=0;c<2;++c) for(int i=0;i<512;++i)
                require(a.getSample(c,i)==b.getSample(c,i),"Disabled effects must preserve the dry signal");
        }
        require(std::abs(bypass.state.getRawParameterValue(effect+"Mix")->load()-.8f)<.001f,"Bypass must preserve the chosen effect mix");
        bypass.state.getParameter(effect+"Enabled")->setValueNotifyingHost(1);
        double difference=0;
        for(int block=0;block<50;++block) {
            bypass.processBlock(a,ma);dry.processBlock(b,mb);
            for(int i=0;i<512;++i) difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));
        }
        require(difference>.01,"Re-enabling an effect must restore its audible processing");
    }
}

static void setActual(ChordHarpProcessor& p,const char* id,float value) {
    auto* parameter=p.state.getParameter(id);require(parameter!=nullptr,"Test parameter must exist");parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static void checkV7Performance() {
    for(int layer=0;layer<4;++layer) {
        auto owner=std::make_unique<ChordHarpProcessor>();auto& synth=*owner;
        for(int c=0;c<4;++c)setActual(synth,synth.mixerParameter(c,"Enabled").toRawUTF8(),c==layer?1.f:0.f);
        setActual(synth,"playMode",1);setActual(synth,"chordSustain",0);setActual(synth,"sampleLoop",1);setActual(synth,"sampleRelease",30);
        if(layer==2){synth.selectSynthSound(true,3);for(int op=1;op<=6;++op)setActual(synth,ChordHarpProcessor::fmOperatorID(true,op-1,"Release").toRawUTF8(),20);}
        synth.prepareToPlay(48000,512);juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;
        // C3 is a real note in CHROMATIC, not a chord selector. Same pitch, different channels.
        midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),100);midi.addEvent(juce::MidiMessage::noteOn(2,60,.8f),120);
        double energy=0;for(int b=0;b<20;++b){synth.processBlock(block,midi);if(b==0)require(block.getMagnitude(0,0,100)==0,"Chromatic notes must respect MIDI sample offsets");energy+=block.getMagnitude(0,512);}
        require(energy>.01,"Every independent layer must play chromatic MIDI notes");
        midi.addEvent(juce::MidiMessage::noteOff(1,60),0);for(int b=0;b<15;++b)synth.processBlock(block,midi);
        require(block.getMagnitude(0,512)>.00001f,"Releasing one MIDI channel must preserve the same note held on another channel");
        midi.addEvent(juce::MidiMessage::controllerEvent(2,64,127),0);midi.addEvent(juce::MidiMessage::noteOff(2,60),50);
        for(int b=0;b<15;++b)synth.processBlock(block,midi);
        require(block.getMagnitude(0,512)>.00001f,"CC64 must hold chromatic notes after note-off");
        midi.addEvent(juce::MidiMessage::controllerEvent(2,64,0),0);
        for(int b=0;b<250;++b)synth.processBlock(block,midi);
        require(block.getMagnitude(0,512)<.00001f,"Pedal-up must release every sustained chromatic voice");
        midi.addEvent(juce::MidiMessage::midiClock(),0);synth.processBlock(block,midi); // one-byte event regression
        if(layer==1) {
            setActual(synth,"organEnabled",0);midi.addEvent(juce::MidiMessage::noteOn(1,62,.9f),0);synth.processBlock(block,midi);
            setActual(synth,"organEnabled",1);for(int b=0;b<20;++b)synth.processBlock(block,midi);
            require(block.getMagnitude(0,512)>.001f,"Enabling a layer must restore notes that are already held");
            setActual(synth,"pad",0);for(int b=0;b<150;++b)synth.processBlock(block,midi);
            require(block.getMagnitude(0,512)<.00001f,"CHORDS must mute the chord layers in CHROMATIC too");
        }
    }
    for(int layer=0;layer<3;++layer) {
        auto plain=std::make_unique<ChordHarpProcessor>(),detuned=std::make_unique<ChordHarpProcessor>();
        for(auto* synth:{plain.get(),detuned.get()}) {
            setActual(*synth,"playMode",1);
            for(int c=0;c<4;++c)setActual(*synth,synth->mixerParameter(c,"Enabled").toRawUTF8(),c==layer?1.f:0.f);
            const juce::String prefix=layer==0?"harp":layer==1?"organ":"fm";
            setActual(*synth,(prefix+"Unison").toRawUTF8(),7);setActual(*synth,(prefix+"Detune").toRawUTF8(),synth==plain.get()?0.f:35.f);setActual(*synth,(prefix+"Drift").toRawUTF8(),0);
            synth->prepareToPlay(48000,512);
        }
        juce::AudioBuffer<float> a(2,512),b(2,512);juce::MidiBuffer ma,mb;ma.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);mb.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
        double difference=0;for(int block=0;block<30;++block){plain->processBlock(a,ma);detuned->processBlock(b,mb);for(int i=0;i<512;++i)difference+=std::abs(a.getSample(0,i)-b.getSample(0,i));}
        require(difference>1,"Moving each layer's Detune parameter must change actual processor audio with seven voices");
    }
    auto owner=std::make_unique<ChordHarpProcessor>();auto& layered=*owner;
    setActual(layered,"playMode",1);setActual(layered,"organEnabled",1);setActual(layered,"fmEnabled",1);setActual(layered,"samplesEnabled",1);setActual(layered,"sampleLoop",1);
    layered.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);midi.addEvent(juce::MidiMessage::noteOn(1,64,.8f),64);
    for(int b=0;b<20;++b)layered.processBlock(audio,midi);
    for(int c=0;c<4;++c)require(layered.mixerLevel(c)>.00001f,"Harp, Organ, FM and Samples must all reach their own mixer meters together");
    layered.computerNoteOn(67);layered.processBlock(audio,midi);layered.computerNoteOff(67);layered.processBlock(audio,midi);
    setActual(layered,"masterWidth",0);for(int b=0;b<40;++b)layered.processBlock(audio,midi);
    for(int i=0;i<512;++i)require(std::abs(audio.getSample(0,i)-audio.getSample(1,i))<.00001f,"Master Width must act on the complete processor output");
    setActual(layered,"tapeEnabled",1);setActual(layered,"tapeNoise",1);
    for(int b=0;b<40;++b)layered.processBlock(audio,midi);
    for(int i=0;i<512;++i)require(std::abs(audio.getSample(0,i)-audio.getSample(1,i))<.00001f,"Master Width must also include the stereo Tape insert");
    setActual(layered,"masterEnabled",0);
    for(int b=0;b<10;++b)layered.processBlock(audio,midi);require(audio.getMagnitude(0,512)==0,"Master power must mute even Tape noise");
    layered.toggleMixerSolo(2);require(layered.state.getRawParameterValue("fmSolo")->load()==1,"Mixer solo must reach its parameter");layered.toggleMixerSolo(7);require(layered.state.getRawParameterValue("fmSolo")->load()==0,"Ctrl Master must clear all solos");
    // A genuine v0.6 document: no independent layer/mixer IDs yet.
    auto old=juce::ValueTree("OmnichordPreset",{{"formatVersion",2},{"name","v6 FM"}});
    const auto add=[&](const char* id,float value){old.appendChild(juce::ValueTree("PARAM",{{"id",id},{"value",value}}),nullptr);};
    add("organModel",3);add("organGain",.6f);add("organUnison",7);add("organDetune",30);add("delayMix",.2f);add("reverbMix",.3f);add("rhythmSend",.5f);
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("v6-migration",".omnipreset",false);juce::String error;
    file.replaceWithText(old.toXmlString());require(layered.loadPreset(file,error),"v0.6 presets must migrate to independent layers");file.deleteFile();
    require(layered.state.getRawParameterValue("fmEnabled")->load()==1 && layered.state.getRawParameterValue("organEnabled")->load()==0
        && layered.state.getRawParameterValue("fmUnison")->load()==7 && std::abs(layered.state.getRawParameterValue("rhythmDelaySend")->load()-.1f)<1e-5f
        && layered.state.getRawParameterValue("delayMix")->load()==1,"Migration must preserve FM, unison and old effect sends");
}

class TransportPlayhead final : public juce::AudioPlayHead {
public:
    double ppq=0,bpm=120;bool playing=false,positionAvailable=true;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setBpm(bpm);p.setIsPlaying(playing);if(positionAvailable)p.setPpqPosition(ppq);return p;}
};
static void checkTransportAndLevels() {
    for(bool loop:{false,true})for(bool position:{false,true}) {
        auto owner=std::make_unique<ChordHarpProcessor>();auto& synth=*owner;TransportPlayhead host;host.positionAvailable=position;
        setActual(synth,"rhythmEnabled",1);setActual(synth,"rhythmHost",1);setActual(synth,"rhythmUseLoop",loop?1.f:0.f);setActual(synth,"harpEnabled",0);
        synth.prepareToPlay(48000,512);synth.setPlayHead(&host);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
        const auto render=[&]{synth.processBlock(audio,midi);if(host.playing)host.ppq+=512*host.bpm/(60*48000.);};
        for(int b=0;b<10;++b)render();require(audio.getMagnitude(0,512)==0 && !synth.rhythmIsRunning(),"Armed rhythm must stay silent while the host is stopped, including hosts without PPQ");
        host.playing=true;double energy=0;for(int b=0;b<25;++b){render();energy+=audio.getMagnitude(0,512);}require(energy>.01 && synth.rhythmIsRunning(),"DAW Play must start both step rhythms and recorded loops automatically");
        host.bpm=137;render();require(synth.getCurrentTempo()==137,"Rhythm tempo must follow a running DAW tempo change");
        if(position){host.ppq=2;render();require(synth.rhythmStep()==8,"A DAW seek must move the plugin to the matching sixteenth note");host.ppq=0;render();require(synth.rhythmStep()==0,"A DAW loop wrap must restart the downbeat");}
        midi.addEvent(juce::MidiMessage::allNotesOff(1),0);render();require(synth.rhythmIsRunning(),"DAW All Notes Off at a loop boundary must not latch the synced rhythm off");
        host.playing=false;midi.addEvent(juce::MidiMessage::allNotesOff(1),0);for(int b=0;b<15;++b)render();require(audio.getMagnitude(0,512)<1e-7 && !synth.rhythmIsRunning() && synth.rhythmStep()==-1,"DAW Stop must release drums and stop the displayed playhead");
        host.ppq=0;host.playing=true;energy=0;for(int b=0;b<10;++b){render();energy+=audio.getMagnitude(0,512);}require(energy>.001,"DAW Play after Stop must restart the armed rhythm");
        setActual(synth,"rhythmEnabled",0);for(int b=0;b<15;++b)render();require(!synth.rhythmIsRunning() && synth.dawIsPlaying() && audio.getMagnitude(0,512)<1e-7,"Turning the rhythm channel off must preserve the real DAW transport display");
        synth.setPlayHead(nullptr);
    }
    {
        auto a=std::make_unique<ChordHarpProcessor>(),b=std::make_unique<ChordHarpProcessor>();TransportPlayhead host;host.playing=true;
        for(auto* synth:{a.get(),b.get()}){setActual(*synth,"harpEnabled",0);setActual(*synth,"rhythmEnabled",1);setActual(*synth,"rhythmDelaySend",.4f);setActual(*synth,"rhythmReverbSend",.3f);setActual(*synth,"tapeEnabled",1);synth->prepareToPlay(48000,512);synth->setPlayHead(&host);}
        juce::AudioBuffer<float> audio(2,512),control(2,512);juce::MidiBuffer events,none;
        for(int block=0;block<60;++block){if(block==30)events.addEvent(juce::MidiMessage::allNotesOff(1),0);a->processBlock(audio,events);b->processBlock(control,none);host.ppq+=512*host.bpm/(60*48000.);if(block>=30)for(int c=0;c<2;++c)for(int i=0;i<512;++i)require(audio.getSample(c,i)==control.getSample(c,i),"DAW All Notes Off must preserve the continuous rhythm, effect tails and Tape history");}
        a->setPlayHead(nullptr);b->setPlayHead(nullptr);
    }
    auto owner=std::make_unique<ChordHarpProcessor>();auto& synth=*owner;
    for(const char* id:{"gain","harpGain","organGain","fmGain","sampleGain","rhythmVolume","delayMix","reverbMix","harpDelaySend"}) {
        auto* parameter=synth.state.getParameter(id);require(parameter->getText(parameter->convertTo0to1(1),0)=="0.0 dB","Unity gain must display zero dB");
        require(parameter->getText(parameter->convertTo0to1(0),0)=="-inf dB","Zero gain must display minus infinity");
        require(std::abs(parameter->convertFrom0to1(parameter->getValueForText("-6 dB"))-.501187f)<1e-5,"Entering dB must map to the corresponding linear audio gain");
    }
    const auto energy=[&](int layer,int sample=-1) {
        for(int c=0;c<4;++c)setActual(synth,synth.mixerParameter(c,"Enabled").toRawUTF8(),c==layer?1.f:0.f);
        setActual(synth,"playMode",1);if(sample>=0)synth.selectSampleSound(sample,true);
        synth.prepareToPlay(48000,512);juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;for(int note:{60,64,67})midi.addEvent(juce::MidiMessage::noteOn(1,note,.8f),0);
        double power=0;for(int b=0;b<70;++b){synth.processBlock(audio,midi);for(int i=0;i<512;++i){const auto x=audio.getSample(0,i);require(std::isfinite(x) && std::abs(x)<1,"Calibrated layers must remain finite and bounded");power+=x*x;}}
        return power;
    };
    const double harp=energy(0),organ=energy(1),fm=energy(2);
    require(organ>harp*.35 && organ<harp*2.5 && fm>harp*.1,"Organ and FM must have useful levels alongside Harp at unity faders");
    for(const auto& sample:synth.sampleChoices()) {
        const double power=energy(3,sample.first);if(!(power<harp*6 && power>harp*.03))std::cerr<<"Sample balance "<<sample.second<<" ratio="<<power/harp<<'\n';
        require(power<harp*6 && power>harp*.03,"Factory chord samples must remain in a comparable level range without flattening their dynamics");
    }
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    checkTransportAndLevels();
    checkV7Performance();
    checkFactorySamples();
    checkFMPerformance();
    checkSamplesAndRhythm();
    checkPresetLibrary();
    checkTempoAndAmbience();
    checkEffectBypass();
    checkChordReleaseAndTape();

    const auto bundledLogo=juce::ImageFileFormat::loadFrom(BinaryData::BlinblinLogo_png,
                                                         BinaryData::BlinblinLogo_pngSize);
    require(bundledLogo.isValid() && bundledLogo.getWidth()==2172 && bundledLogo.getHeight()==724,
            "Original BLINBLIN PNG must be bundled and readable");
    const auto whiteLogo=juce::ImageFileFormat::loadFrom(BinaryData::BlinblinLogo_white_png,BinaryData::BlinblinLogo_white_pngSize);
    require(whiteLogo.isValid() && whiteLogo.hasAlphaChannel() && whiteLogo.getBounds()==bundledLogo.getBounds(),"Both original logo variants must be embedded with transparency and matching dimensions");
    require(bundledLogo.hasAlphaChannel(), "Original logo transparency must be preserved");
    auto processorOwner=std::make_unique<ChordHarpProcessor>();auto& processor=*processorOwner;
    require(processor.getName()=="Omnichord","Public plugin name must be Omnichord");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    processor.prepareToPlay(48000, 512);
    // Ableton C1/C2/C3 select columns; these events must not pluck the harp.
    for(int octave=0;octave<4;++octave) {
        midi.addEvent(juce::MidiMessage::noteOn(1,24+octave*12,0.8f),0);
        processor.processBlock(audio,midi);
        require(processor.selectedChord()==1+octave*12,"MIDI row starts at Db in the matching quality");
        require(audio.getMagnitude(0,512)==0,"Chord selectors must not trigger harp voices");
        midi.addEvent(juce::MidiMessage::noteOn(1,35+octave*12,0.8f),0);
        processor.processBlock(audio,midi);
        require(processor.selectedChord()==6+octave*12,"MIDI row ends at F# in the matching quality");
    }
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Processor must start silent");

    midi.addEvent(juce::MidiMessage::noteOn(1, 84, 1.0f), 128);
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 0, 128) == 0, "MIDI must respect its sample offset");
    require(audio.getMagnitude(0, 128, 384) > 0.001f, "MIDI must produce audio");
    for (int i = 0; i < 512; ++i)
        require(audio.getSample(0, i) == audio.getSample(1, i), "Both output channels must render");

    midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "All Sound Off must silence voices");

    processor.strum(3);
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 512) > 0.001f, "UI strum queue must produce audio");

    auto* tone = processor.state.getParameter("tone");
    tone->setValueNotifyingHost(0.8f);
    juce::Image logo(juce::Image::ARGB, 64, 24, true);
    juce::Graphics logoGraphics(logo);
    logoGraphics.setColour(juce::Colours::black);
    logoGraphics.fillEllipse(2, 2, 20, 20);
    juce::MemoryOutputStream logoBytes;
    juce::PNGImageFormat png;
    require(png.writeImageToStream(logo, logoBytes), "Test logo must encode");
    const auto logoData=logoBytes.getMemoryBlock().toBase64Encoding();
    processor.state.state.setProperty("customLogo",logoData,nullptr);
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    auto restoredOwner=std::make_unique<ChordHarpProcessor>();auto& restored=*restoredOwner;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    require(std::abs(restored.state.getRawParameterValue("tone")->load() - 0.8f) < 0.001f,
            "DAW state must restore parameters");
    require(!restored.state.state.hasProperty("customLogo"),
            "Legacy custom logo overrides must be removed; branding is fixed");
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    require(editor && editor->getWidth() == 1060 && editor->getHeight() == 805,
            "Editor must instantiate at its intended dimensions");
    editor.reset();
    // A fresh instance must display the bundled logo without any project override.
    restored.state.state.removeProperty("customLogo",nullptr);
    editor.reset(restored.createEditor());
    // Foleys creates the toolbox using callAsync; exercise the real GUI message loop.
    bool guiReady=false;
    juce::MessageManager::callAsync([&guiReady] { guiReady=true; });
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    require(guiReady,"GUI callbacks must finish before inspecting the Foleys toolbox");
    {
        auto* panel=dynamic_cast<ChordHarpEditor*>(editor.get())->getGUIBuilder().findGuiItemWithId("sound-controls");
        require(dynamic_cast<ChordHarpEditor*>(editor.get())->getGUIBuilder().findGuiItemWithId("mixer-controls")->isVisible(),"A fresh editor must show the Mixer page without clicking a tab");
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-mixer-panel-preview.png").createOutputStream();
        require(stream && png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds()),*stream),"The initial mixer screenshot must render");
    }
    const auto preview=editor->createComponentSnapshot(editor->getLocalBounds());
    float darkest=1.0f, lightest=0.0f;
    auto* logoWidget=dynamic_cast<ChordHarpEditor*>(editor.get())->getGUIBuilder().findGuiItemWithId("logo")->getWrappedComponent();
    const auto logoBounds=editor->getLocalArea(logoWidget,logoWidget->getLocalBounds());
    for(int y=logoBounds.getY();y<logoBounds.getBottom();++y) for(int x=logoBounds.getX();x<logoBounds.getRight();++x) {
        const auto brightness=preview.getPixelAt(x,y).getBrightness();
        darkest=juce::jmin(darkest,brightness);
        lightest=juce::jmax(lightest,brightness);
    }
    require(lightest-darkest>0.25f, "Bundled logo must be visible against the cream panel");
    auto previewFile=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-preview.png");
    auto previewStream=previewFile.createOutputStream();
    require(previewStream && png.writeImageToStream(preview,*previewStream), "Editor preview must render");

    auto* magicEditor=dynamic_cast<ChordHarpEditor*>(editor.get());
    require(magicEditor!=nullptr,"Foleys editor must be created");
    auto& builder=magicEditor->getGUIBuilder();
    require(builder.findGuiItemWithId("logo-picker")==nullptr
        && !builder.getFactoryNames().contains("LogoPicker"),"Logo replacement must not be exposed");
    auto* gainItem=builder.findGuiItemWithId("gain");
    auto* strumItem=builder.findGuiItemWithId("strum");
    auto* chordItem=builder.findGuiItemWithId("chord-13");
    require(gainItem && strumItem && chordItem,"XML must create the instrument controls");
    require(dynamic_cast<StrumStrip*>(strumItem->getWrappedComponent())!=nullptr,
            "Custom strum component must be registered in Foleys");
    auto* gainSlider=dynamic_cast<juce::Slider*>(gainItem->getWrappedComponent());
    require(gainSlider!=nullptr,"Foleys must create the volume slider");
    gainSlider->setValue(0.4,juce::sendNotificationSync);
    require(std::abs(restored.state.getRawParameterValue("gain")->load()-0.4f)<0.001f,
            "Visual slider must control the audio parameter");
    auto* chordButton=dynamic_cast<juce::TextButton*>(chordItem->getWrappedComponent());
    require(chordButton!=nullptr,"Foleys must create chord buttons");
    chordButton->setState(juce::Button::buttonDown); chordButton->setState(juce::Button::buttonNormal);
    require(restored.state.getRawParameterValue("chord")->load()==13.0f,
            "Chord button must set the chord parameter");
    auto* silenceItem=builder.findGuiItemWithId("silence");
    auto* silence=silenceItem?dynamic_cast<juce::TextButton*>(silenceItem->getWrappedComponent()):nullptr;
    require(silence && silence->onClick,"Silence trigger must be connected");
    silence->onClick();
    require(restored.panic.load(),"Silence trigger must reach the audio processor");

    auto shell=restored.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    auto gainNode=shell.getChildWithProperty("id","gain");
    require(gainNode.isValid(),"XML must expose the gain layout node");
    restored.state.getParameter("organEnabled")->setValueNotifyingHost(1);
    restored.prepareToPlay(48000,512);
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    builder.getMagicToolBox().setVisible(false);
#endif
    const auto mods=juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier;
    require(magicEditor->keyPressed(juce::KeyPress('A',juce::ModifierKeys(mods),0)),"Ctrl+Shift+A selects DbMaj7");
    require(restored.selectedChord()==37,"Mac Maj7 must use the fourth chord row");
    magicEditor->keyStateChanged(false);
    require(magicEditor->keyPressed(juce::KeyPress(static_cast<int>(U'Ñ'),juce::ModifierKeys(juce::ModifierKeys::shiftModifier),0)),"Shift+enye selects F#Maj7");
    require(restored.selectedChord()==42,"Mac keyboard follows the F#Maj7 column mapping");
    magicEditor->keyStateChanged(false);
    require(magicEditor->keyPressed(juce::KeyPress('1',juce::ModifierKeys(mods),0)),"Mac numbers must play harp while Ctrl+Shift is held for Maj7");
    restored.processBlock(audio,midi); restored.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"Mac keyboard must produce harp audio");
    magicEditor->keyStateChanged(false);
    auto* pad=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId("pad")->getWrappedComponent());
    require(pad!=nullptr,"Standalone organ toggle must exist");
    restored.panic.store(true); restored.processBlock(audio,midi);
    pad->setToggleState(true,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("pad")->load()>0.5f,"Clicking the organ toggle must stay ON");
    for(int n=0;n<20;++n) restored.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"Enabling the organ from the GUI must produce audio without host automation");
    pad->setToggleState(false,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("pad")->load()<0.5f,"Clicking the organ toggle must switch OFF");
    auto* sustainSwitch=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId("chordSustain")->getWrappedComponent());
    sustainSwitch->setToggleState(false,juce::sendNotificationSync); pad->setToggleState(true,juce::sendNotificationSync);
    chordButton->setState(juce::Button::buttonDown);
    for(int b=0;b<20;++b) restored.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"Holding a chord button must sound with Sustain OFF");
    chordButton->setState(juce::Button::buttonNormal);
    for(int b=0;b<120;++b) restored.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)<0.00001f,"Releasing a chord button must release the organ with Sustain OFF");
    sustainSwitch->setToggleState(true,juce::sendNotificationSync); pad->setToggleState(false,juce::sendNotificationSync);
    auto* maj7=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId("chord-37")->getWrappedComponent());
    maj7->setState(juce::Button::buttonDown); maj7->setState(juce::Button::buttonNormal);
    require(restored.selectedChord()==37 && restored.state.getParameter("chord")->getNormalisableRange().end==35,"Maj7 buttons preserve legacy chord automation range");
    chordButton->setState(juce::Button::buttonDown); chordButton->setState(juce::Button::buttonNormal); require(restored.selectedChord()==13,"Selecting a regular chord clears Maj7");
    auto* theme=dynamic_cast<juce::ComboBox*>(builder.findGuiItemWithId("theme")->getWrappedComponent());
    require(builder.findGuiItemWithId("tape-controls") && builder.findGuiItemWithId("chordSustain"),"Tape tab and chord sustain switch must exist");
    require(pad->getButtonText()=="CHORDS" && static_cast<bool>(pad->getProperties()["physical-switch"]),"Organ must use a physical push button");
    auto* follow=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId("harpQuantize")->getWrappedComponent());
    require(follow && follow->getButtonText()=="FOLLOW CHORDS","Harp quantize must be a physical FOLLOW CHORDS button");
    require(theme && theme->getNumItems()==6,"Theme selector must expose six palettes");
    checkLogoThemes(restored,*magicEditor);
    auto customPalette=restored.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
    customPalette.setProperty("color-ffeee6cf","fff0ddbb",nullptr);
    const auto panelBefore=shell.getProperty("background-color");
    const auto originalX=gainNode.getProperty("pos-x");
    theme->setSelectedId(3,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    auto palette=restored.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
    require(restored.state.getRawParameterValue("theme")->load()==2,"Theme dropdown must set the selected theme");
    waitForGui([&]{return palette.getProperty("color-ffeee6cf").toString()=="ff242c40";},"Midnight must recolour the actual panel palette");
    require(shell.getProperty("background-color")==panelBefore && gainNode.getProperty("pos-x")==originalX,"Themes preserve layout positions and palette references");
    require(pad->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xfff0ca59) && follow->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xfff0ca59),"Organ and Follow Chords stay yellow in every theme");
    require(sustainSwitch->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xff96a9ef),"Sustain button must use the active theme colour");
    auto nightPreview=editor->createComponentSnapshot(editor->getLocalBounds());
    require(nightPreview.getPixelAt(35,180)!=preview.getPixelAt(35,180),"Changing themes must visibly recolour the rendered instrument");
    auto nightStream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-midnight-preview.png").createOutputStream();
    require(nightStream && png.writeImageToStream(nightPreview,*nightStream),"Midnight preview must render");
    auto* fmItem=builder.findGuiItemWithId("fm-editor");
    require(fmItem && dynamic_cast<FMPanel*>(fmItem->getWrappedComponent()),"The six-operator FM editor must exist");
    auto* fm=fmItem->getWrappedComponent();
    auto* target=dynamic_cast<juce::ComboBox*>(findComponent(*fm,"fm-target"));
    auto* op=dynamic_cast<juce::ComboBox*>(findComponent(*fm,"fm-operator"));
    auto* algorithm=dynamic_cast<juce::ComboBox*>(findComponent(*fm,"fm-algorithm"));
    auto* ratio=dynamic_cast<juce::Slider*>(findComponent(*fm,"fm-ratio"));
    require(target && op && algorithm && ratio,"Operator selection, algorithm and ratio must be editable");
    auto* advanced=dynamic_cast<juce::TextButton*>(findComponent(*fm,"fm-advanced"));auto* simple=findComponent(*fm,"fm-simple");
    require(advanced && simple && simple->isVisible() && !ratio->isVisible(),"FM must open in its simplified knob view");
    auto* simpleCutoff=dynamic_cast<juce::Slider*>(findComponent(*fm,"fm-simple-filter"));simpleCutoff->setValue(3500,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("fmCutoff")->load()==3500,"The simplified Filter knob must control the same engine parameter as Advanced");
    const auto review=[&](const char* name) {
        auto* tabs=findTabBar(*builder.findGuiItemWithId("sound-controls"));tabs->setCurrentTabIndex(3);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        auto* panel=builder.findGuiItemWithId("sound-controls");juce::PNGImageFormat format;
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(name).createOutputStream();require(stream!=nullptr,"The FM review image must open");stream->setPosition(0);stream->truncate();
        require(format.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds()),*stream),"Both FM views must render for review");
    };
    review("Omnichord-fm-simple-panel-preview.png");
    advanced->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    require(!simple->isVisible() && ratio->isVisible(),"AVANZADO must reveal the six-operator editor");review("Omnichord-fm-advanced-panel-preview.png");

    target->setSelectedId(2,juce::sendNotificationSync);op->setSelectedId(6,juce::sendNotificationSync);
    ratio->setValue(3.5,juce::sendNotificationSync);algorithm->setSelectedId(32,juce::sendNotificationSync);
    require(std::abs(restored.state.getRawParameterValue("organOp6Ratio")->load()-3.5f)<.001f
        && restored.state.getRawParameterValue("organAlgorithm")->load()==32,"The FM editor must bind the selected bank and operator");
    const float ratioBefore=restored.state.getRawParameterValue("organOp6Ratio")->load();
    advanced->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    require(simple->isVisible() && !ratio->isVisible() && restored.state.getRawParameterValue("organOp6Ratio")->load()==ratioBefore,"Closing Advanced must preserve the edited patch");
    restored.selectSynthSound(true,4);
    require(restored.getSynthChoice(true)==4 && restored.state.getRawParameterValue("fmEnabled")->load()==1,"Selecting an FM sound must load its operators and activate FM");
    restored.state.getParameter("organModel")->setValueNotifyingHost(0);
    auto* tabs=findTabBar(*builder.findGuiItemWithId("sound-controls"));
    require(tabs && tabs->getNumTabs()==8,"The interface must expose the Tape tab");
    require(builder.findGuiItemWithId("preset-bar") && builder.findGuiItemWithId("instrument-body"),"The preset library and physical case must be present");
    auto* shellItem=builder.findGuiItemWithId("shell");auto* panelItem=builder.findGuiItemWithId("sound-controls");
    require(shellItem->getLocalBounds().reduced(24).contains(shellItem->getLocalArea(panelItem,panelItem->getLocalBounds())),"The tabbed panel must stay inside the instrument with a safe edge margin");
    for(auto page:shell.getChildWithProperty("id","sound-controls")) {
        auto* pageItem=builder.findGuiItemWithId(page.getProperty("id").toString());
        for(auto node:page) if(auto* control=builder.findGuiItemWithId(node.getProperty("id").toString())) {
            const auto bounds=pageItem->getLocalArea(control,control->getLocalBounds());
            if(!pageItem->getLocalBounds().contains(bounds)) std::cerr << "Control outside tab: " << node.getProperty("id").toString() << ' ' << bounds.toString() << " in " << pageItem->getLocalBounds().toString() << '\n';
            require(pageItem->getLocalBounds().contains(bounds),"Every tab control must remain inside its panel");
        }
    }
    // Layer LEDs share mixer power without selecting their tab.
    auto* mixer=builder.findGuiItemWithId("mixer")->getWrappedComponent();
    auto* readout=dynamic_cast<juce::Label*>(findComponent(*mixer,"mixer-db-0"));require(readout!=nullptr,"Mixer must show persistent dB readouts");
    readout->setText("-6 dB",juce::sendNotificationSync);require(std::abs(restored.state.getRawParameterValue("harpGain")->load()-.501187f)<1e-5,"The editable dB readout must set actual channel volume");
    require(static_cast<bool>(findComponent(*mixer,"harpPan")->getProperties()["centre-fill"]),"Pan must use a centre-origin colour fill");
    require(static_cast<bool>(findComponent(*mixer,"masterWidth")->getProperties()["width-spread"]),"Width must use symmetric expanding dots");
    require(mixer && findComponent(*mixer,"delayWidth") && findComponent(*mixer,"reverbWidth") && findComponent(*mixer,"masterWidth"),"Mixer must expose Width for both returns and Master");
    for(int channel=0;channel<8;++channel) {
        auto* button=dynamic_cast<juce::TextButton*>(findComponent(*mixer,"mixer-power-"+juce::String(channel)));
        require(button!=nullptr,"Each mixer channel must have a physical power button");
        button->setToggleState(false,juce::sendNotificationSync);
        require(restored.state.getRawParameterValue(restored.mixerParameter(channel,"Enabled"))->load()==0,"Mixer power must switch its parameter off");
        button->setToggleState(true,juce::sendNotificationSync);
        require(restored.state.getRawParameterValue(restored.mixerParameter(channel,"Enabled"))->load()==1,"Mixer power must switch its parameter on");
    }
    for(const auto* id:{"organEnabled","delayEnabled","reverbEnabled"}) {
        auto* led=dynamic_cast<juce::TextButton*>(findComponent(*editor,"tab-led-"+juce::String(id)));
        require(led!=nullptr,"Activatable tabs must have clickable LEDs");const int selected=tabs->getCurrentTabIndex();
        led->setToggleState(false,juce::sendNotificationSync);require(restored.state.getRawParameterValue(id)->load()==0,"LED must switch its layer off");
        led->setToggleState(true,juce::sendNotificationSync);require(restored.state.getRawParameterValue(id)->load()==1,"LED must switch its layer on");
        require(tabs->getCurrentTabIndex()==selected,"Using the LED must leave the current tab selected");
    }
    for(const auto* id:{"harpUnison","organUnison"}) {
        auto* voices=dynamic_cast<juce::ComboBox*>(builder.findGuiItemWithId(id)->getWrappedComponent());
        require(voices && voices->getNumItems()==7,"Foleys Voices selector must contain all seven choices");
        voices->setSelectedId(7,juce::sendNotificationSync);require(restored.state.getRawParameterValue(id)->load()==7,"Selecting seven voices must set the actual engine value to seven");
    }
    auto* fmVoices=dynamic_cast<juce::ComboBox*>(findComponent(*fm,"fm-voices"));
    require(fmVoices && fmVoices->getNumItems()==7,"FM must expose all seven unison voices too");
    fmVoices->setSelectedId(7,juce::sendNotificationSync);require(restored.state.getRawParameterValue("fmUnison")->load()==7,"FM voices must bind the independent FM engine");
    auto* modes=builder.findGuiItemWithId("play-modes")->getWrappedComponent();
    auto* mode=dynamic_cast<juce::TextButton*>(findComponent(*modes,"mode-chromatic"));require(mode!=nullptr,"CHROMATIC paper label must exist");
    mode->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);require(restored.chromaticMode(),"Paper label must select CHROMATIC");
    dynamic_cast<juce::TextButton*>(findComponent(*modes,"mode-chord"))->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(100);require(!restored.chromaticMode(),"Paper label must return to CHORD");
    tabs->setCurrentTabIndex(0);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    {
        const auto snapshot=panelItem->createComponentSnapshot(panelItem->getLocalBounds());
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-mixer-panel-preview.png").createOutputStream();
        require(stream!=nullptr,"Mixer review image must open");stream->setPosition(0);stream->truncate();
        require(png.writeImageToStream(snapshot,*stream),"The mixer panel must render for visual review");
    }
    tabs->setCurrentTabIndex(6);
    auto* fxTabs=findTabBar(*builder.findGuiItemWithId("fx-controls"));require(fxTabs && fxTabs->getNumTabs()==3,"FX must contain Delay, Reverb and Tape");fxTabs->setCurrentTabIndex(0);
    auto* syncButton=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId("delaySync")->getWrappedComponent());
    auto* timing=builder.findGuiItemWithId("delayTime")->getWrappedComponent();
    require(syncButton && timing,"Delay sync and time controls must exist");
    syncButton->setToggleState(true,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
    require(findComponent(*timing,"delay-note")->isVisible() && !findComponent(*timing,"delay-ms")->isVisible(),"Sync must replace milliseconds with a note selector");
    auto* division=dynamic_cast<juce::ComboBox*>(findComponent(*timing,"delay-note"));
    division->setSelectedId(9,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("delayDivision")->load()==8,"Triplet selection must reach the delay engine");
    syncButton->setToggleState(false,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
    require(findComponent(*timing,"delay-ms")->isVisible() && !findComponent(*timing,"delay-note")->isVisible(),"Free delay must show milliseconds");
    fxTabs->setCurrentTabIndex(2);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    require(tabs->getCurrentTabIndex()==6 && builder.findGuiItemWithId("tape-controls")->isVisible(),"Selecting an FX subtab must keep the main FX page selected and visible");
    const auto tapePreview=editor->createComponentSnapshot(editor->getLocalBounds());
    auto tapeStream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-tape-preview.png").createOutputStream();
    require(tapeStream && png.writeImageToStream(tapePreview,*tapeStream),"Tape panel preview must render");
    for(const auto& page:std::array<std::pair<int,const char*>,3>{{{3,"fm"},{4,"samples"},{5,"rhythms"}}}) {
        tabs->setCurrentTabIndex(page.first);juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        const auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-"+juce::String(page.second)+"-preview.png").createOutputStream();
        require(stream && png.writeImageToStream(snapshot,*stream),"The FM, sample and rhythm pages must render for review");
    }
    auto* rhythmPanel=builder.findGuiItemWithId("rhythmpanel")->getWrappedComponent();
    auto* loopSwitch=dynamic_cast<juce::TextButton*>(findComponent(*rhythmPanel,"rhythm-loop-mode"));
    require(loopSwitch!=nullptr,"Recorded loops must have a visible mode selector");
    loopSwitch->setToggleState(true,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("rhythmUseLoop")->load()==1,"The LOOP button must activate the recorded-loop engine");
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    {
        const auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-loop-preview.png").createOutputStream();
        require(stream && png.writeImageToStream(snapshot,*stream),"The recorded-loop panel must render for review");
    }
    loopSwitch->setToggleState(false,juce::sendNotificationSync);
    tabs->setCurrentTabIndex(0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    theme->setSelectedId(1,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    waitForGui([&]{return palette.getProperty("color-ffeee6cf").toString()=="fff0ddbb";},"Personalizado must recover manual palette edits after switching themes");
    theme->setSelectedId(3,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    maj7->setState(juce::Button::buttonDown); maj7->setState(juce::Button::buttonNormal);
    gainNode.setProperty("pos-x",123,&builder.getUndoManager());
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    builder.setEditMode(true);
    require(builder.isEditModeOn(),"Visual edit mode must be available");
    builder.setEditMode(false);
    auto layoutFile=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-layout-test.xml");
    require(builder.getMagicToolBox().saveGUI(layoutFile),"Foleys Save XML must export the layout");
    gainNode.setProperty("pos-x",300,nullptr);
    builder.getMagicToolBox().loadGUI(layoutFile);
    auto reloadedShell=restored.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    require(static_cast<int>(reloadedShell.getChildWithProperty("id","gain").getProperty("pos-x"))==123,
            "Foleys Load XML must recover the exported position");
#endif
    restored.state.getParameter("delayMix")->setValueNotifyingHost(0.35f);
    restored.state.getParameter("reverbMix")->setValueNotifyingHost(0.25f);
    restored.state.getParameter("tapeEnabled")->setValueNotifyingHost(1);
    restored.state.getParameter("tapeNoise")->setValueNotifyingHost(0.4f);
    restored.state.getParameter("tapeWow")->setValueNotifyingHost(0.6f);
    restored.state.getParameter("tapeFlutter")->setValueNotifyingHost(0.2f);
    restored.state.getParameter("tapeDrive")->setValueNotifyingHost(0.7f);
    juce::MemoryBlock layoutState;
    restored.getStateInformation(layoutState);
    auto sessionRestoredOwner=std::make_unique<ChordHarpProcessor>();auto& sessionRestored=*sessionRestoredOwner;
    sessionRestored.setStateInformation(layoutState.getData(),static_cast<int>(layoutState.getSize()));
    require(sessionRestored.selectedChord()==37 && sessionRestored.state.getRawParameterValue("theme")->load()==2,"Sessions must restore Maj7 and colour theme");
    require(sessionRestored.state.getRawParameterValue("tapeEnabled")->load()==1
        && std::abs(sessionRestored.state.getRawParameterValue("tapeNoise")->load()-0.4f)<0.001f
        && std::abs(sessionRestored.state.getRawParameterValue("tapeWow")->load()-0.6f)<0.001f
        && std::abs(sessionRestored.state.getRawParameterValue("tapeFlutter")->load()-0.2f)<0.001f
        && std::abs(sessionRestored.state.getRawParameterValue("tapeDrive")->load()-0.7f)<0.001f,"All tape controls must survive session restore");
    auto restoredShell=sessionRestored.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    require(static_cast<int>(restoredShell.getChildWithProperty("id","gain").getProperty("pos-x"))==123,
            "DAW sessions must preserve visual layout edits");
    require(std::abs(sessionRestored.state.getRawParameterValue("delayMix")->load()-0.35f)<0.001f
        && std::abs(sessionRestored.state.getRawParameterValue("reverbMix")->load()-0.25f)<0.001f,
        "Effects must survive DAW state restoration");
    auto legacySessionOwner=std::make_unique<ChordHarpProcessor>();auto& legacySession=*legacySessionOwner;
    auto legacyLayout=legacySession.magicState.getGuiTree().createCopy();
    auto legacyRoot=legacyLayout.getChildWithName("View");
    legacyRoot.setProperty("layout-version",8,nullptr);
    auto legacyShell=legacyRoot.getChildWithProperty("id","shell");
    for(auto node:legacyShell) if(node.getProperty("id").toString().startsWith("chord-") || node.getProperty("id").toString().startsWith("quality-")) node.setProperty("pos-y",static_cast<int>(node.getProperty("pos-y"))+44,nullptr);
    auto legacyTabs=legacyShell.getChildWithProperty("id","sound-controls");
    legacyTabs.removeChild(legacyTabs.getChildWithProperty("id","fm-controls"),nullptr);
    legacyShell.getChildWithProperty("id","gain").setProperty("pos-x",321,nullptr);
    legacyShell.appendChild(juce::ValueTree("LogoPicker",{{"id","logo-picker"}}),nullptr);
    legacySession.magicState.setGuiValueTree(legacyLayout);
    legacySession.state.getParameter("gain")->setValueNotifyingHost(0.23f);
    juce::MemoryBlock oldSession;
    legacySession.getStateInformation(oldSession);
    auto upgradedOwner=std::make_unique<ChordHarpProcessor>();auto& upgraded=*upgradedOwner;
    upgraded.setStateInformation(oldSession.getData(),static_cast<int>(oldSession.getSize()));
    auto upgradedShell=upgraded.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    require(upgradedShell.getChildWithProperty("id","sound-controls").isValid()
        && !upgradedShell.getChildWithProperty("id","logo-picker").isValid(),"Legacy layouts must gain v2 controls and lose logo replacement");
    require(upgradedShell.getChildWithProperty("id","chord-47").isValid() && static_cast<int>(upgradedShell.getChildWithProperty("id","gain").getProperty("pos-x"))==321,"Upgrading the synthesis panel preserves the user layout");
    require(upgradedShell.getChildWithProperty("id","chordSustain").hasType("OmniSwitch")
        && upgradedShell.getChildWithProperty("id","pad").getProperty("text").toString()=="CHORDS"
        && upgradedShell.getChildWithProperty("id","sound-controls").getChildWithProperty("id","fx-controls").getChildWithProperty("id","fx-tabs").getChildWithProperty("id","tape-controls").isValid(),"Old layouts must gain Tape and physical sustain controls");
    require(std::abs(upgraded.state.getRawParameterValue("gain")->load()-0.23f)<0.001f,"Layout migration must preserve sound parameters");
    // Compare a muted source with and without effects: only the effects may keep sounding.
    const auto tailEnergy=[](bool effects) {
        ChordHarpProcessor synth;
        synth.state.getParameter("delayTime")->setValueNotifyingHost(synth.state.getParameter("delayTime")->convertTo0to1(100));
        synth.state.getParameter("harpDelaySend")->setValueNotifyingHost(effects?0.6f:0.0f);
        synth.state.getParameter("harpReverbSend")->setValueNotifyingHost(effects?0.35f:0.0f);
        synth.prepareToPlay(48000,512);
        juce::AudioBuffer<float> block(2,512); juce::MidiBuffer events;
        events.addEvent(juce::MidiMessage::noteOn(1,72,1.0f),0);
        synth.processBlock(block,events);
        synth.state.getParameter("harpGain")->setValueNotifyingHost(0);
        double energy=0;
        for(int b=0;b<60;++b) {
            synth.processBlock(block,events);
            if(b>20) for(int c=0;c<2;++c) for(int i=0;i<512;++i) energy+=block.getSample(c,i)*block.getSample(c,i);
        }
        events.addEvent(juce::MidiMessage::allSoundOff(1),0); synth.processBlock(block,events);
        require(block.getMagnitude(0,512)==0,"MIDI panic must clear effect tails");
        return energy;
    };
    const double dryTail=tailEnergy(false),wetTail=tailEnergy(true);
    require(wetTail>0.0001 && wetTail>dryTail*10,"Delay and reverb must produce audible tails after source mute");
    require(backgroundPassed,"Every host and theme must render the same opaque neutral outer background");
    processor.releaseResources();
    std::cout << "Processor checks passed: MIDI, audio, logo, opaque themes, visual controls, layout state, complete presets, startup, host tempo and reverb\n";
}
