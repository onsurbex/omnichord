#pragma once
#include "PluginProcessor.h"
#include "Themes.h"
class DelayTimeControl final : public juce::Component,private juce::Timer {
public:
    explicit DelayTimeControl(ChordHarpProcessor& p):processor(p) {
        time.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); time.setTextBoxStyle(juce::Slider::TextBoxBelow,false,84,20);
        time.setTextValueSuffix(" ms"); time.setComponentID("delay-ms"); division.setComponentID("delay-note");
        addAndMakeVisible(time);addAndMakeVisible(division);addAndMakeVisible(tempo);
        division.addItemList(p.state.getParameter("delayDivision")->getAllValueStrings(),1);
        timeAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("delayTime"),time);
        divisionAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*p.state.getParameter("delayDivision"),division);
        tempo.setFont(juce::FontOptions(10.0f));tempo.setJustificationType(juce::Justification::centred);
        timerCallback();startTimerHz(10);
    }
    void resized() override { time.setBounds(getLocalBounds());division.setBounds(0,25,getWidth(),27);tempo.setBounds(0,62,getWidth(),20); }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    void timerCallback() override {
        const bool sync=processor.state.getRawParameterValue("delaySync")->load()>.5f;
        time.setVisible(!sync);division.setVisible(sync);tempo.setVisible(sync);
        const auto& theme=omnichord::themes[static_cast<size_t>(juce::jlimit(0,5,static_cast<int>(processor.state.getRawParameterValue("theme")->load())))];
        const auto palette=processor.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
        const auto colour=[&](const char* name,juce::uint32 fallback) { return juce::Colour::fromString(palette.getProperty(name,juce::Colour(fallback).toString()).toString()); };
        const auto ink=colour("color-ff263e38",theme.ink),accent=colour("color-ffb66b44",theme.accent),button=colour("color-ffdad3bc",theme.button);
        time.setColour(juce::Slider::thumbColourId,ink);time.setColour(juce::Slider::rotarySliderFillColourId,accent);
        time.setColour(juce::Slider::textBoxTextColourId,ink);time.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);time.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        division.setColour(juce::ComboBox::backgroundColourId,button);division.setColour(juce::ComboBox::textColourId,ink);
        tempo.setColour(juce::Label::textColourId,ink);
        tempo.setText(juce::String(processor.tempoComesFromHost()?"DAW ":"")+juce::String(processor.getCurrentTempo(),1)+" BPM",juce::dontSendNotification);
    }
    ChordHarpProcessor& processor;
    juce::Slider time;juce::ComboBox division;juce::Label tempo;
    std::unique_ptr<juce::SliderParameterAttachment> timeAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> divisionAttachment;
};
