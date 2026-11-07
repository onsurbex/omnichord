#pragma once
#include <JuceHeader.h>
#include <foleys_gui_magic/foleys_gui_magic.h>
#include "Engine.h"
#include "Delay.h"
#include "Tape.h"
#include "ChordGate.h"
#include "Ambience.h"
#include "Tempo.h"
#include "SynthPatches.h"
#include "Rhythm.h"
#include "SampleLibrary.h"
#include "SampleChords.h"
#include <atomic>

class ChordHarpProcessor final : public juce::AudioProcessor {
public:
    explicit ChordHarpProcessor(juce::File presetRoot={});
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
    double getTailLengthSeconds() const override;
    double getCurrentTempo() const { return currentTempo.load(); }
    bool tempoComesFromHost() const { return hostTempo.load(); }
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
    void pressChord(int value,bool computer=false);
    void releaseChord(int value,bool computer=false);
    bool computerKeyboardEnabled() const;
    void setChordFromMidi(int value);
    juce::AudioProcessorValueTreeState state;
    foleys::MagicProcessorState magicState{*this};
    std::atomic<bool> panic{false};
    bool savePreset(const juce::File&,juce::String& error);
    bool loadPreset(const juce::File&,juce::String& error);
    bool loadFactoryPreset(int index,juce::String& error);
    bool setStartupPreset(juce::String& error);
    bool clearStartupPreset(juce::String& error);
    juce::File getPresetDirectory() const { return presetHome.getChildFile("Presets"); }
    juce::String getPresetName() const { const juce::ScopedLock lock(presetLock); return presetName; }
    juce::String getPresetSource() const { const juce::ScopedLock lock(presetLock); return presetSource; }
    unsigned getPresetRevision() const { return presetRevision.load(); }
    static juce::StringArray factoryPresetNames();
    static juce::String fmOperatorID(bool organ,int op,const juce::String& field);
    juce::StringArray synthSoundNames(bool organ) const;
    chordharp::FMPatch getFMPatch(bool organ) const;
    int getSynthChoice(bool organ) const;
    void selectSynthSound(bool organ,int choice);
    bool loadSampleLibrary(const juce::File&,juce::String& error);
    juce::StringArray sampleNames() const;
    std::vector<std::pair<int,juce::String>> sampleChoices(bool loops=false) const;
    std::vector<std::pair<int,juce::String>> drumChoices() const;
    void selectSampleSound(int index);
    juce::String sampleLibraryPath() const;
    juce::String sampleLibraryReference() const;
    unsigned getSampleRevision() const {return sampleRevision.load();}
    int rhythmStep() const {return playingStep.load();}
    static juce::String rhythmStepID(int track,int step);
    void editRhythmStep(int track,int step,bool on);
private:
    static juce::File defaultPresetHome();
    juce::File presetHome;
    juce::ValueTree factoryPreset;
    juce::String presetName="Inicio",presetSource="factory:0";
    std::atomic<unsigned> presetRevision{0};
    std::atomic<bool> presetReset{false};
    mutable juce::CriticalSection presetLock;
    void updatePresetName(const juce::String&,const juce::String& source="project");
    juce::ValueTree capturePreset(const juce::String&) const;
    bool applyPreset(const juce::ValueTree&,juce::String&);
    bool writePreset(const juce::File&,const juce::ValueTree&,juce::String&) const;
    void loadStartupPreset();
    bool restoreSampleLibrary(const juce::String&,juce::String&);
    void readSynthParameters();
    struct SynthReferences {
        std::array<std::atomic<float>*,9> common{};
        std::array<std::array<std::atomic<float>*,8>,6> op{};
    };
    std::array<SynthReferences,2> synthRefs{};
    std::array<std::atomic<bool>,2> synthUpdating{};
private:
    enum Param { Gain,Tone,Sustain,Vibrato,Chord,Pad,HarpGain,OrganGain,
        OrganTone,OrganOctave,HarpSound,HarpAttack,HarpQuantize,DelayTime,DelayFeedback,
        DelayMix,ReverbSize,ReverbDamping,ReverbMix,ComputerKeys,Major7,Theme,ChordSustain,TapeEnabled,TapeNoise,TapeWow,TapeFlutter,TapeDrive,OrganModel,ReverbPredelay,ReverbLow,ReverbMid,ReverbHigh,ReverbEarly,DelaySync,DelayDivision,DelayTempo,DelayEnabled,ReverbEnabled,NumParams };
    std::array<std::atomic<float>*,NumParams> raw{};
    juce::RangedAudioParameter* chordParameter=nullptr;
    juce::RangedAudioParameter* major7Parameter=nullptr;
    std::atomic<bool> chordRetrigger{false};
    chordharp::Rhythm rhythm;
    chordharp::SampleChords sampleChords;
    chordharp::SynthFilter sampleFilter;
    chordharp::DCBlocker sampleDC;
    std::unique_ptr<SampleLibrary> sampleLibrary;
    juce::String factoryLibraryPath;
    std::atomic<unsigned> sampleRevision{0};
    std::atomic<int> playingStep{-1};
    mutable juce::CriticalSection libraryLock;
    bool rhythmMuted=false,lastRhythmEnabled=false;
    std::array<std::atomic<float>*,11> rhythmRaw{};
    std::array<std::array<std::atomic<float>*,16>,5> stepRaw{};
    std::array<std::atomic<float>*,5> trackRaw{},sampleRaw{};
    void readRhythmParameters(bool,double,bool);
    chordharp::StereoDelay delay;
    chordharp::Tape tape;
    chordharp::ChordGate heldChords;
    juce::MidiKeyboardState guiChordKeys;
    juce::MidiBuffer guiChordMidi;
    bool tapeMuted=false,lastChordSustain=true;
    chordharp::Ambience reverb;
    std::atomic<double> currentTempo{120};
    std::atomic<bool> hostTempo{false};
    juce::SmoothedValue<float> masterGain;
    bool organSilenced=false,lastPad=false;
    int lastChord=0;
    static juce::AudioProcessorValueTreeState::ParameterLayout parameters();
    chordharp::Engine engine;
    juce::AbstractFifo fifo{128};
    std::array<int,128> notes{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordHarpProcessor)
};
