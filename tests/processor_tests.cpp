#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "BinaryData.h"
#include "../Source/Themes.h"
#include "AmbienceChecks.h"
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
#include <foleys_gui_magic/Editor/foleys_ToolBox.h>
#endif
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
static bool backgroundPassed=true;

static juce::TabbedButtonBar* findTabBar(juce::Component& parent) {
    if(auto* tabs=dynamic_cast<juce::TabbedButtonBar*>(&parent)) return tabs;
    for(int i=0;i<parent.getNumChildComponents();++i)
        if(auto* tabs=findTabBar(*parent.getChildComponent(i))) return tabs;
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
        require(white?brightness/pixels>0.8:brightness/pixels<0.2,"Rendered logo must match the contrast of the panel");
    };
    auto* parameter=processor.state.getParameter("theme");
    for(int theme=0;theme<6;++theme) {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(theme)));
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        assertLogo(theme==2);
        const auto full=editor.createComponentSnapshot(editor.getLocalBounds());
        if(full.getPixelAt(0,0)!=juce::Colour(0xff292a2d) || full.getPixelAt(full.getWidth()-1,full.getHeight()-1)!=juce::Colour(0xff292a2d)) {
            backgroundPassed=false;
            std::cerr << "Theme " << theme << " snapshot " << full.getWidth() << 'x' << full.getHeight() << " editor " << editor.getBounds().toString()
                      << " opaque=" << editor.isOpaque() << " outer corners: " << full.getPixelAt(0,0).toString() << ", " << full.getPixelAt(full.getWidth()-1,full.getHeight()-1).toString() << '\n';
        }
        for(const auto* id:{"pad","harpQuantize","chordSustain","tapeEnabled","delayEnabled","reverbEnabled"})
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
    const auto bad=root.getChildFile("corrupt.omnipreset");bad.replaceWithText("<OmnichordPreset formatVersion=\"1\"><PARAM id=\"gain\" value=\"0.1\"/><PARAM id=\"tone\" value=\"nan\"/></OmnichordPreset>");
    require(!next.loadPreset(bad,error),"Invalid preset values must be rejected");
    require(std::abs(next.getParameters()[0]->getValue()-expected[0])<.001f,"A corrupt preset must not partially change the sound");
    for(auto* parameter:source.getParameters()) parameter->setValueNotifyingHost(0);
    require(source.savePreset(root.getChildFile("minimum.omnipreset"),error) && source.loadPreset(root.getChildFile("minimum.omnipreset"),error),"Parameter minima must survive preset encoding");
    juce::MemoryBlock project;source.getStateInformation(project);next.setStateInformation(project.getData(),static_cast<int>(project.getSize()));
    require(next.state.getRawParameterValue("gain")->load()==0,"DAW project state must override the startup preset");
    for(int i=0;i<ChordHarpProcessor::factoryPresetNames().size();++i) require(next.loadFactoryPreset(i,error),"Every factory preset must load");
    require(source.magicState.getGuiTree().toXmlString()==layout,"Loading sound presets must preserve the edited panel layout");
    require(source.clearStartupPreset(error),"The default can be returned to factory settings");
    ChordHarpProcessor factory(root);require(factory.state.getRawParameterValue("organModel")->load()==4 && factory.state.getRawParameterValue("pad")->load()==0,"Factory startup must be deterministic and silent");
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

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto bundledLogo=juce::ImageFileFormat::loadFrom(BinaryData::BlinblinLogo_png,
                                                         BinaryData::BlinblinLogo_pngSize);
    require(bundledLogo.isValid() && bundledLogo.getWidth()==2172 && bundledLogo.getHeight()==724,
            "Original BLINBLIN PNG must be bundled and readable");
    const auto whiteLogo=juce::ImageFileFormat::loadFrom(BinaryData::BlinblinLogo_white_png,BinaryData::BlinblinLogo_white_pngSize);
    require(whiteLogo.isValid() && whiteLogo.hasAlphaChannel() && whiteLogo.getBounds()==bundledLogo.getBounds(),"Both original logo variants must be embedded with transparency and matching dimensions");
    require(bundledLogo.hasAlphaChannel(), "Original logo transparency must be preserved");
    ChordHarpProcessor processor;
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
    ChordHarpProcessor restored;
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
    require(pad->getButtonText()=="ORGAN" && static_cast<bool>(pad->getProperties()["physical-switch"]),"Organ must use a physical push button");
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
    require(palette.getProperty("color-ffeee6cf").toString()=="ff242c40","Midnight must recolour the actual panel palette");
    require(shell.getProperty("background-color")==panelBefore && gainNode.getProperty("pos-x")==originalX,"Themes preserve layout positions and palette references");
    require(pad->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xfff0ca59) && follow->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xfff0ca59),"Organ and Follow Chords stay yellow in every theme");
    require(sustainSwitch->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xff96a9ef),"Sustain button must use the active theme colour");
    auto nightPreview=editor->createComponentSnapshot(editor->getLocalBounds());
    require(nightPreview.getPixelAt(35,180)!=preview.getPixelAt(35,180),"Changing themes must visibly recolour the rendered instrument");
    auto nightStream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-midnight-preview.png").createOutputStream();
    require(nightStream && png.writeImageToStream(nightPreview,*nightStream),"Midnight preview must render");
    auto* tabs=findTabBar(*builder.findGuiItemWithId("sound-controls"));
    require(tabs && tabs->getNumTabs()==6,"The interface must expose the Tape tab");
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
    for(const auto* id:{"delayEnabled","reverbEnabled"}) {
        auto* button=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId(id)->getWrappedComponent());
        require(button && static_cast<bool>(button->getProperties()["physical-switch"]),"Delay and reverb must have physical bypass buttons");
        button->setToggleState(false,juce::sendNotificationSync);
        require(restored.state.getRawParameterValue(id)->load()==0,"Effect button must switch processing off");
        button->setToggleState(true,juce::sendNotificationSync);
        require(restored.state.getRawParameterValue(id)->load()==1,"Effect button must switch processing back on");
    }
    tabs->setCurrentTabIndex(2);
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
    tabs->setCurrentTabIndex(4);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    const auto tapePreview=editor->createComponentSnapshot(editor->getLocalBounds());
    auto tapeStream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-tape-preview.png").createOutputStream();
    require(tapeStream && png.writeImageToStream(tapePreview,*tapeStream),"Tape panel preview must render");
    tabs->setCurrentTabIndex(0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    theme->setSelectedId(1,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    require(palette.getProperty("color-ffeee6cf").toString()=="fff0ddbb","Personalizado must recover manual palette edits after switching themes");
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
    ChordHarpProcessor sessionRestored;
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
    ChordHarpProcessor legacySession;
    auto legacyLayout=legacySession.magicState.getGuiTree().createCopy();
    auto legacyRoot=legacyLayout.getChildWithName("View");
    legacyRoot.setProperty("layout-version",2,nullptr);
    auto legacyShell=legacyRoot.getChildWithProperty("id","shell");
    for(int chord=36;chord<48;++chord) legacyShell.removeChild(legacyShell.getChildWithProperty("id","chord-"+juce::String(chord)),nullptr);
    legacyShell.removeChild(legacyShell.getChildWithProperty("id","quality-3"),nullptr);
    legacyShell.removeChild(legacyShell.getChildWithProperty("id","chordSustain"),nullptr);
    auto legacyTabs=legacyShell.getChildWithProperty("id","sound-controls");
    legacyTabs.removeChild(legacyTabs.getChildWithProperty("id","tape-controls"),nullptr);
    legacyShell.getChildWithProperty("id","gain").setProperty("pos-x",321,nullptr);
    legacyShell.appendChild(juce::ValueTree("LogoPicker",{{"id","logo-picker"}}),nullptr);
    legacySession.magicState.setGuiValueTree(legacyLayout);
    legacySession.state.getParameter("gain")->setValueNotifyingHost(0.23f);
    juce::MemoryBlock oldSession;
    legacySession.getStateInformation(oldSession);
    ChordHarpProcessor upgraded;
    upgraded.setStateInformation(oldSession.getData(),static_cast<int>(oldSession.getSize()));
    auto upgradedShell=upgraded.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    require(upgradedShell.getChildWithProperty("id","sound-controls").isValid()
        && !upgradedShell.getChildWithProperty("id","logo-picker").isValid(),"Legacy layouts must gain v2 controls and lose logo replacement");
    require(upgradedShell.getChildWithProperty("id","chord-47").isValid() && static_cast<int>(upgradedShell.getChildWithProperty("id","gain").getProperty("pos-x"))==321,"V2 migration adds Maj7 while preserving the user layout");
    require(upgradedShell.getChildWithProperty("id","chordSustain").hasType("OmniSwitch")
        && upgradedShell.getChildWithProperty("id","pad").getProperty("text").toString()=="ORGAN"
        && upgradedShell.getChildWithProperty("id","sound-controls").getChildWithProperty("id","tape-controls").isValid(),"Old layouts must gain Tape and physical sustain controls");
    require(std::abs(upgraded.state.getRawParameterValue("gain")->load()-0.23f)<0.001f,"Layout migration must preserve sound parameters");
    // Compare a muted source with and without effects: only the effects may keep sounding.
    const auto tailEnergy=[](bool effects) {
        ChordHarpProcessor synth;
        synth.state.getParameter("delayTime")->setValueNotifyingHost(synth.state.getParameter("delayTime")->convertTo0to1(100));
        synth.state.getParameter("delayMix")->setValueNotifyingHost(effects?0.6f:0.0f);
        synth.state.getParameter("reverbMix")->setValueNotifyingHost(effects?0.35f:0.0f);
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
    checkPresetLibrary();
    checkTempoAndAmbience();
    checkEffectBypass();
    checkChordReleaseAndTape();
    require(backgroundPassed,"Every host and theme must render the same opaque neutral outer background");
    processor.releaseResources();
    std::cout << "Processor checks passed: MIDI, audio, logo, opaque themes, visual controls, layout state, complete presets, startup, host tempo and reverb\n";
}
