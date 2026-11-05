#pragma once
#include <JuceHeader.h>
#include <foleys_gui_magic/foleys_gui_magic.h>
#include "Engine.h"
#include "Delay.h"
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
    const juce::String getName() const override { return "Omnichord"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Classic"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void strum(int index);
    void selectChord(int value);
    int selectedChord() const;
    bool computerKeyboardEnabled() const;
    juce::AudioProcessorValueTreeState state;
    foleys::MagicProcessorState magicState{*this};
    std::atomic<bool> panic{false};
private:
    enum Param { Gain,Tone,Sustain,Vibrato,Chord,Pad,HarpGain,OrganGain,OrganSound,
        OrganTone,OrganOctave,HarpSound,HarpAttack,HarpQuantize,DelayTime,DelayFeedback,
        DelayMix,ReverbSize,ReverbDamping,ReverbMix,ComputerKeys,NumParams };
    std::array<std::atomic<float>*,NumParams> raw{};
    juce::RangedAudioParameter* chordParameter=nullptr;
    chordharp::StereoDelay delay;
    juce::Reverb reverb;
    juce::SmoothedValue<float> masterGain;
    bool organSilenced=false,lastPad=false;
    int lastChord=0;
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters();
    chordharp::Engine engine;
    juce::AbstractFifo fifo{128};
    std::array<int,128> notes{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordHarpProcessor)
};
