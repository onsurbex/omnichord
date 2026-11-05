#pragma once
#include "PluginProcessor.h"
#include <map>

class StrumStrip final : public juce::Component, public juce::SettableTooltipClient {
public:
    enum Colours { backgroundColour=0x2010000, stringColour, activeColour };
    explicit StrumStrip(ChordHarpProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override { previous=-1; play(e.y); }
    void mouseDrag(const juce::MouseEvent& e) override { play(e.y); }
    void mouseUp(const juce::MouseEvent&) override { previous=-1; repaint(); }
    void configure(foleys::GuiItem&);
    static std::vector<std::pair<juce::String,int>> colours();
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&);
private:
    void play(int y);
    ChordHarpProcessor& processor;
    int previous=-1;
    float stringWidth=1.4f;
};

class InstrumentLook final : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
};

class ChordHarpEditor final : public foleys::MagicPluginEditor,private juce::Timer {
public:
    explicit ChordHarpEditor(ChordHarpProcessor&);
    ~ChordHarpEditor() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;
private:
    void releaseComputerKeys();
    void timerCallback() override;
    int currentTheme=-1;
    juce::ValueTree themedLayout;
    ChordHarpProcessor& processor;
    std::map<int,int> heldKeys;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordHarpEditor)
};
