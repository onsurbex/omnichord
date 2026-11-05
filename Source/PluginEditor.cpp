#include "PluginEditor.h"

void StrumStrip::paint(juce::Graphics& g) {
    g.setColour(juce::Colour(0xff292e2d)); g.fillRoundedRectangle(getLocalBounds().toFloat(),12);
    for(int i=0;i<12;++i) {
        const float x=(static_cast<float>(i)+0.5f)*getWidth()/12.0f;
        g.setColour(i==previous ? juce::Colour(0xffedb46d):juce::Colour(0xffa8aaa0));
        g.fillRoundedRectangle(x-2,18,4,static_cast<float>(getHeight()-36),2);
    }
}
void StrumStrip::play(int x) {
    const int current=juce::jlimit(0,11,x*12/juce::jmax(1,getWidth()));
    if(previous==current) return;
    if(previous<0) processor.strum(current);
    else {
        const int step=current>previous ? 1:-1;
        for(int i=previous+step;i!=current+step;i+=step) processor.strum(i);
    }
    previous=current; repaint();
}
ChordHarpEditor::ChordHarpEditor(ChordHarpProcessor& p):AudioProcessorEditor(p),processor(p),strip(p) {
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffd8d4c4));
    look.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffc17a43));
    look.setColour(juce::TextButton::textColourOffId,juce::Colour(0xff242b29));
    look.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffad6c40));
    look.setColour(juce::Slider::thumbColourId,juce::Colour(0xff242b29));
    look.setColour(juce::ToggleButton::textColourId,juce::Colour(0xff242b29));
    setLookAndFeel(&look);
    const char* names[]{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    for(int i=0;i<36;++i) {
        auto& button=chords[static_cast<size_t>(i)];
        button.setButtonText(juce::String(names[i%12])+(i/12==1?"m":i/12==2?"7":""));
        button.onClick=[this,i] {
            auto* parameter=processor.state.getParameter("chord");
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(i))); parameter->endChangeGesture();
            timerCallback();
        };
        addAndMakeVisible(button);
    }
    const char* ids[]{"gain","tone","sustain","vibrato"};
    const char* titles[]{"VOLUME","TONE","SUSTAIN","VIBRATO"};
    for(size_t i=0;i<4;++i) {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,65,20);
        knobs[i].setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xff242b29));
        addAndMakeVisible(knobs[i]);
        labels[i].setText(titles[i],juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId,juce::Colour(0xff242b29));
        addAndMakeVisible(labels[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,ids[i],knobs[i]);
    }
    addAndMakeVisible(strip); addAndMakeVisible(pad); addAndMakeVisible(stop);
    padAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"pad",pad);
    stop.onClick=[this] {
        auto* parameter=processor.state.getParameter("pad");
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture();
        processor.panic.store(true);
    };
    setSize(900,530); startTimerHz(15); timerCallback();
}
ChordHarpEditor::~ChordHarpEditor() { stopTimer(); setLookAndFeel(nullptr); }
void ChordHarpEditor::timerCallback() {
    const int selected=static_cast<int>(processor.state.getRawParameterValue("chord")->load());
    for(int i=0;i<36;++i) chords[static_cast<size_t>(i)].setToggleState(i==selected,juce::dontSendNotification);
}
void ChordHarpEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xffeee8d6));
    g.setColour(juce::Colour(0xff242b29)); g.setFont(32.0f);
    g.drawText("CHORDHARP",30,20,400,45,juce::Justification::centredLeft);
    g.setFont(13.0f); g.drawText("CLASSIC ELECTRONIC CHORD & HARP  /  0.1",32,65,500,25,juce::Justification::centredLeft);
    g.drawText("CHORD MEMORY  /  MAJOR · MINOR · SEVENTH",32,230,650,25,juce::Justification::centredLeft);
    g.drawText("STRUM  /  DRAG ACROSS THE STRINGS",32,386,650,25,juce::Justification::centredLeft);
}
void ChordHarpEditor::resized() {
    for(int i=0;i<4;++i) { labels[static_cast<size_t>(i)].setBounds(30+i*150,100,125,22); knobs[static_cast<size_t>(i)].setBounds(30+i*150,122,125,100); }
    pad.setBounds(660,126,200,35); stop.setBounds(680,176,150,30);
    for(int i=0;i<36;++i) chords[static_cast<size_t>(i)].setBounds(30+(i%12)*70,260+(i/12)*40,64,34);
    strip.setBounds(30,418,834,80);
}
