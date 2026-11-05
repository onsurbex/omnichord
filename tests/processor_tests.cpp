#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "BinaryData.h"
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
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    processor.prepareToPlay(48000, 512);
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Processor must start silent");

    midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 128);
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
    require(restored.state.state.getProperty("customLogo").toString()==logoData,
            "Custom logo must survive DAW state restoration");
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    require(editor && editor->getWidth() == 1060 && editor->getHeight() == 650,
            "Editor must instantiate at its intended dimensions");
    editor.reset();
    // A fresh instance must display the bundled logo without any project override.
    restored.state.state.removeProperty("customLogo",nullptr);
    editor.reset(restored.createEditor());
    const auto preview=editor->createComponentSnapshot(editor->getLocalBounds());
    float darkest=1.0f, lightest=0.0f;
    for(int y=49;y<135;++y) for(int x=660;x<940;++x) {
        const auto brightness=preview.getPixelAt(x,y).getBrightness();
        darkest=juce::jmin(darkest,brightness);
        lightest=juce::jmax(lightest,brightness);
    }
    require(lightest-darkest>0.25f, "Bundled logo must be visible against the cream panel");
    auto previewFile=juce::File::getCurrentWorkingDirectory().getChildFile("ChordHarp-preview.png");
    auto previewStream=previewFile.createOutputStream();
    require(previewStream && png.writeImageToStream(preview,*previewStream), "Editor preview must render");

    auto* magicEditor=dynamic_cast<ChordHarpEditor*>(editor.get());
    require(magicEditor!=nullptr,"Foleys editor must be created");
    auto& builder=magicEditor->getGUIBuilder();
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
    chordButton->setToggleState(true,juce::sendNotificationSync);
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
    gainNode.setProperty("pos-x",123,&builder.getUndoManager());
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    builder.setEditMode(true);
    require(builder.isEditModeOn(),"Visual edit mode must be available");
    builder.setEditMode(false);
    auto layoutFile=juce::File::getCurrentWorkingDirectory().getChildFile("ChordHarp-layout-test.xml");
    require(builder.getMagicToolBox().saveGUI(layoutFile),"Foleys Save XML must export the layout");
    gainNode.setProperty("pos-x",300,nullptr);
    builder.getMagicToolBox().loadGUI(layoutFile);
    auto reloadedShell=restored.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    require(static_cast<int>(reloadedShell.getChildWithProperty("id","gain").getProperty("pos-x"))==123,
            "Foleys Load XML must recover the exported position");
#endif
    juce::MemoryBlock layoutState;
    restored.getStateInformation(layoutState);
    ChordHarpProcessor sessionRestored;
    sessionRestored.setStateInformation(layoutState.getData(),static_cast<int>(layoutState.getSize()));
    auto restoredShell=sessionRestored.magicState.getGuiTree().getChildWithName("View").getChildWithProperty("id","shell");
    require(static_cast<int>(restoredShell.getChildWithProperty("id","gain").getProperty("pos-x"))==123,
            "DAW sessions must preserve visual layout edits");
    processor.releaseResources();
    std::cout << "Processor checks passed: MIDI, audio, logo, visual controls, XML export/import and layout state\n";
}
