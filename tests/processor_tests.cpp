#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "BinaryData.h"
#include "../Source/Themes.h"
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
#include <foleys_gui_magic/Editor/foleys_ToolBox.h>
#endif
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto bundledLogo=juce::ImageFileFormat::loadFrom(BinaryData::BlinblinLogo_png,
                                                         BinaryData::BlinblinLogo_pngSize);
    require(bundledLogo.isValid() && bundledLogo.getWidth()==2172 && bundledLogo.getHeight()==724,
            "Original BLINBLIN PNG must be bundled and readable");
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
    require(editor && editor->getWidth() == 1060 && editor->getHeight() == 695,
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
    for(int y=49;y<135;++y) for(int x=660;x<940;++x) {
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
    chordButton->onClick();
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
    require(magicEditor->keyPressed(juce::KeyPress('1')),"Mac number keys play the harp");
    restored.processBlock(audio,midi); restored.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"Mac keyboard must produce harp audio");
    magicEditor->keyStateChanged(false);
    auto* pad=dynamic_cast<juce::ToggleButton*>(builder.findGuiItemWithId("pad")->getWrappedComponent());
    require(pad!=nullptr,"Standalone organ toggle must exist");
    restored.panic.store(true); restored.processBlock(audio,midi);
    pad->setToggleState(true,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("pad")->load()>0.5f,"Clicking the organ toggle must stay ON");
    for(int n=0;n<20;++n) restored.processBlock(audio,midi);
    require(audio.getMagnitude(0,512)>0.001f,"Enabling the organ from the GUI must produce audio without host automation");
    pad->setToggleState(false,juce::sendNotificationSync);
    require(restored.state.getRawParameterValue("pad")->load()<0.5f,"Clicking the organ toggle must switch OFF");
    auto* maj7=dynamic_cast<juce::TextButton*>(builder.findGuiItemWithId("chord-37")->getWrappedComponent());
    maj7->onClick();
    require(restored.selectedChord()==37 && restored.state.getParameter("chord")->getNormalisableRange().end==35,"Maj7 buttons preserve legacy chord automation range");
    chordButton->onClick(); require(restored.selectedChord()==13,"Selecting a regular chord clears Maj7");
    auto* theme=dynamic_cast<juce::ComboBox*>(builder.findGuiItemWithId("theme")->getWrappedComponent());
    require(theme && theme->getNumItems()==6,"Theme selector must expose six palettes");
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
    auto nightPreview=editor->createComponentSnapshot(editor->getLocalBounds());
    require(nightPreview.getPixelAt(35,180)!=preview.getPixelAt(35,180),"Changing themes must visibly recolour the rendered instrument");
    auto nightStream=juce::File::getCurrentWorkingDirectory().getChildFile("Omnichord-midnight-preview.png").createOutputStream();
    require(nightStream && png.writeImageToStream(nightPreview,*nightStream),"Midnight preview must render");
    theme->setSelectedId(1,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    require(palette.getProperty("color-ffeee6cf").toString()=="fff0ddbb","Personalizado must recover manual palette edits after switching themes");
    theme->setSelectedId(3,juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    maj7->onClick();
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
    juce::MemoryBlock layoutState;
    restored.getStateInformation(layoutState);
    ChordHarpProcessor sessionRestored;
    sessionRestored.setStateInformation(layoutState.getData(),static_cast<int>(layoutState.getSize()));
    require(sessionRestored.selectedChord()==37 && sessionRestored.state.getRawParameterValue("theme")->load()==2,"Sessions must restore Maj7 and colour theme");
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
    processor.releaseResources();
    std::cout << "Processor checks passed: MIDI, audio, logo, visual controls, XML export/import and layout state\n";
}
