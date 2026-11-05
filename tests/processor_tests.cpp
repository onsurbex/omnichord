#include "../Source/PluginProcessor.h"
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
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
    // Capture the actual JUCE editor, with the honest default wordmark.
    restored.state.state.removeProperty("customLogo",nullptr);
    editor.reset(restored.createEditor());
    const auto preview=editor->createComponentSnapshot(editor->getLocalBounds());
    auto previewFile=juce::File::getCurrentWorkingDirectory().getChildFile("ChordHarp-preview.png");
    auto previewStream=previewFile.createOutputStream();
    require(previewStream && png.writeImageToStream(preview,*previewStream), "Editor preview must render");
    processor.releaseResources();
    std::cout << "Processor checks passed: MIDI timing, audio, strum, silence, state, logo and editor\n";
}
