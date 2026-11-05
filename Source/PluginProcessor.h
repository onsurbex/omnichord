#pragma once
#include <JuceHeader.h>
#include "Engine.h"
#include <atomic>

class ChordHarpProcessor final : public juce::AudioProcessor {
public:
    ChordHarpProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ChordHarp"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 60; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Classic"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void strum(int index);
    juce::AudioProcessorValueTreeState state;
    std::atomic<bool> panic{false};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters();
    chordharp::Engine engine;
    juce::AbstractFifo fifo{128};
    std::array<int,128> notes{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordHarpProcessor)
};
