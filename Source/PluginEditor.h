#pragma once
#include "PluginProcessor.h"

class StrumStrip final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit StrumStrip(ChordHarpProcessor& p):processor(p) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { previous=-1; play(e.y); }
    void mouseDrag(const juce::MouseEvent& e) override { play(e.y); }
    void mouseUp(const juce::MouseEvent&) override { previous=-1; repaint(); }
private:
    void play(int y);
    ChordHarpProcessor& processor;
    int previous=-1;
};
class InstrumentLook final : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
class ChordHarpEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ChordHarpEditor(ChordHarpProcessor&);
    ~ChordHarpEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void chooseLogo();
    ChordHarpProcessor& processor;
    InstrumentLook look;
    StrumStrip strip;
    std::array<juce::TextButton,36> chords;
    std::array<juce::Slider,4> knobs;
    std::array<juce::Label,4> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> attachments;
    juce::ToggleButton pad{"ORGANO DE ACORDES"};
    juce::TextButton stop{"SILENCIO"};
    juce::TextButton logoButton{"CARGAR LOGO"};
    juce::Image logo;
    juce::String logoData;
    std::unique_ptr<juce::FileChooser> logoChooser;
    juce::TooltipWindow tooltips{this, 650};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> padAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordHarpEditor)
};
