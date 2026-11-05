#pragma once
#include "PluginProcessor.h"

class StrumStrip final : public juce::Component {
public:
    explicit StrumStrip(ChordHarpProcessor& p):processor(p) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { previous=-1; play(e.x); }
    void mouseDrag(const juce::MouseEvent& e) override { play(e.x); }
    void mouseUp(const juce::MouseEvent&) override { previous=-1; repaint(); }
private:
    void play(int x);
    ChordHarpProcessor& processor;
    int previous=-1;
};
class ChordHarpEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ChordHarpEditor(ChordHarpProcessor&);
    ~ChordHarpEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    ChordHarpProcessor& processor;
    juce::LookAndFeel_V4 look;
    StrumStrip strip;
    std::array<juce::TextButton,36> chords;
    std::array<juce::Slider,4> knobs;
    std::array<juce::Label,4> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> attachments;
    juce::ToggleButton pad{"CHORD ORGAN"};
    juce::TextButton stop{"SILENCE"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> padAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordHarpEditor)
};
