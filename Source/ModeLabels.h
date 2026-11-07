#pragma once
#include "PluginProcessor.h"
#include "SampleRhythmEditor.h"

class PaperModeButton final : public juce::TextButton {
public:
    explicit PaperModeButton(const juce::String& text):TextButton(text){setWantsKeyboardFocus(false);setClickingTogglesState(true);}
    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto r=getLocalBounds().toFloat().reduced(2);juce::Path paper;
        paper.startNewSubPath(r.getX()+3,r.getY()+1);paper.lineTo(r.getRight()-2,r.getY());paper.lineTo(r.getRight(),r.getBottom()-3);
        paper.lineTo(r.getRight()-5,r.getBottom());paper.lineTo(r.getX(),r.getBottom()-1);paper.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(.3f));g.fillPath(paper,juce::AffineTransform::translation(0,2));
        g.setColour(hover?juce::Colour(0xff27272b):juce::Colour(0xff151518));g.fillPath(paper);
        g.setColour(juce::Colours::white.withAlpha(.15f));g.strokePath(paper,juce::PathStrokeType(.6f));
        for(int i=0;i<5;++i){g.setColour(juce::Colours::white.withAlpha(.025f));g.drawLine(r.getX()+3,r.getY()+5+i*4,r.getRight()-3,r.getY()+4+i*4,.5f);}
        g.saveState();g.addTransform(juce::AffineTransform::rotation(-.025f,r.getCentreX(),r.getCentreY()));
        g.setColour(juce::Colours::white);g.setFont(juce::FontOptions("Marker Felt",18.f,juce::Font::bold));
        g.drawFittedText(getButtonText(),getLocalBounds().reduced(5,4).translated(0,down?1:0),juce::Justification::centred,1);g.restoreState();
        if(getToggleState()){g.setColour(findColour(juce::TextButton::buttonOnColourId));g.drawLine(r.getX()+8,r.getBottom()-3,r.getRight()-5,r.getBottom()-4,2.3f);}
    }
};
class ModeLabels final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;PaperModeButton chord{"CHORD"},chromatic{"CHROMATIC"};
public:
    explicit ModeLabels(ChordHarpProcessor& p):processor(p) {
        chord.setComponentID("mode-chord");chromatic.setComponentID("mode-chromatic");addAndMakeVisible(chord);addAndMakeVisible(chromatic);
        const auto select=[this](int mode){auto* param=processor.state.getParameter("playMode");param->beginChangeGesture();param->setValueNotifyingHost(static_cast<float>(mode));param->endChangeGesture();};
        chord.onClick=[select]{select(0);};chromatic.onClick=[select]{select(1);};
        chord.setTooltip("CHORD: octavas C0-B3 seleccionan acordes; C4+ toca el arpa.");chromatic.setTooltip("CHROMATIC: MIDI y teclado Mac tocan notas normales en las capas activas. La botonera conserva sus acordes.");
        timerCallback();startTimerHz(15);
    }
    void resized() override {const int w=static_cast<int>(getWidth()*.42);chord.setBounds(0,0,w,getHeight());chromatic.setBounds(w+3,0,getWidth()-w-3,getHeight());}
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours(){return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&){return {};}
private:
    void timerCallback() override {const bool mode=processor.chromaticMode();chord.setToggleState(!mode,juce::dontSendNotification);chromatic.setToggleState(mode,juce::dontSendNotification);const auto c=sampleui::palette(processor,"color-ffb66b44");chord.setColour(juce::TextButton::buttonOnColourId,c);chromatic.setColour(juce::TextButton::buttonOnColourId,c);}
};
