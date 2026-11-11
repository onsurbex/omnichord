#pragma once
#include "PluginProcessor.h"
#include "Themes.h"
class DelayTimeControl final : public juce::Component,private juce::Timer {
public:
    explicit DelayTimeControl(ChordHarpProcessor& p):processor(p) {
        time.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); time.setTextBoxStyle(juce::Slider::TextBoxBelow,false,84,20);
        time.setTextValueSuffix(" ms"); time.setComponentID("delay-ms"); division.setComponentID("delay-note");
        division.setLookAndFeel(&displayLook);
        addAndMakeVisible(time);addAndMakeVisible(division);addAndMakeVisible(tempo);
        division.addItemList(p.state.getParameter("delayDivision")->getAllValueStrings(),1);
        timeAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("delayTime"),time);
        divisionAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*p.state.getParameter("delayDivision"),division);
        tempo.setFont(juce::FontOptions(10.0f));tempo.setJustificationType(juce::Justification::centred);
        timerCallback();startTimerHz(10);
    }
    ~DelayTimeControl() override {division.setLookAndFeel(nullptr);}
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
        const auto ink=colour("color-ff263e38",theme.ink),accent=colour("color-ffb66b44",theme.accent);
        const juce::Colour displayBackground(0xff35383d),displayText(0xfff1f2f5);
        time.setColour(juce::Slider::thumbColourId,ink);time.setColour(juce::Slider::rotarySliderFillColourId,accent);
        time.setColour(juce::Slider::textBoxTextColourId,displayText);time.setColour(juce::Slider::textBoxBackgroundColourId,displayBackground);time.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        division.setColour(juce::ComboBox::backgroundColourId,displayBackground);division.setColour(juce::ComboBox::textColourId,displayText);division.setColour(juce::ComboBox::arrowColourId,displayText);
        displayLook.setColour(juce::PopupMenu::backgroundColourId,displayBackground);displayLook.setColour(juce::PopupMenu::textColourId,displayText);
        displayLook.setColour(juce::PopupMenu::highlightedBackgroundColourId,accent);displayLook.setColour(juce::PopupMenu::highlightedTextColourId,accent.contrasting());
        tempo.setColour(juce::Label::backgroundColourId,displayBackground);tempo.setColour(juce::Label::textColourId,displayText);
        tempo.setText(juce::String(processor.tempoComesFromHost()?"DAW ":"")+juce::String(processor.getCurrentTempo(),1)+" BPM",juce::dontSendNotification);
    }
    ChordHarpProcessor& processor;
    juce::LookAndFeel_V4 displayLook;
    juce::Slider time;juce::ComboBox division;juce::Label tempo;
    std::unique_ptr<juce::SliderParameterAttachment> timeAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> divisionAttachment;
};
