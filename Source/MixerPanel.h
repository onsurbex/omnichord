#pragma once
#include "PluginProcessor.h"
#include "SampleRhythmEditor.h"

class MixerSwitch final : public juce::TextButton {
    ChordHarpProcessor& processor;int channel;
public:
    MixerSwitch(ChordHarpProcessor& p,int index,const juce::String& text):TextButton(text),processor(p),channel(index) {
        setClickingTogglesState(true);setWantsKeyboardFocus(false);getProperties().set("physical-switch",true);getProperties().set("compact-switch",true);
        setTooltip(index==7?"Master. Ctrl+clic: quitar todos los solos.":"Activar canal. Ctrl+clic: solo.");
    }
    void mouseDown(const juce::MouseEvent& e) override {
        soloClick=e.mods.isCtrlDown();if(soloClick){processor.toggleMixerSolo(channel);return;}TextButton::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override {if(soloClick){soloClick=false;return;}TextButton::mouseUp(e);}
private: bool soloClick=false;
};
class MixerPanel final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;
    std::array<juce::Slider,8> faders;
    std::array<juce::Slider,5> pans,delaySends,reverbSends;
    std::array<juce::Slider,3> widths;
    std::array<std::unique_ptr<MixerSwitch>,8> buttons;
    juce::TextButton tape{"Tape"};
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;
    std::array<float,8> peak{};
    std::array<juce::Label,8> levels;
    void attach(juce::Slider& slider,const juce::String& id) {
        slider.setComponentID(id);addAndMakeVisible(slider);
        sliderAttachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*processor.state.getParameter(id),slider));
    }
    static void knob(juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);slider.setPopupDisplayEnabled(true,true,nullptr);
    }
public:
    explicit MixerPanel(ChordHarpProcessor& p):processor(p) {
        const char* names[]{"Harp","Organ","FM","Samples","Ritmos","Delay","Rev","MSTR"};
        for(int c=0;c<8;++c) {
            auto& f=faders[static_cast<size_t>(c)];f.setSliderStyle(juce::Slider::LinearVertical);f.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);f.setPopupDisplayEnabled(true,true,nullptr);
            attach(f,p.mixerParameter(c,"Gain"));f.setTooltip(juce::String(names[c])+" - volumen en dB");
            auto& level=levels[static_cast<size_t>(c)];level.setComponentID("mixer-db-"+juce::String(c));level.setFont(juce::FontOptions(9.f));level.setJustificationType(juce::Justification::centred);level.setEditable(false,true,false);addAndMakeVisible(level);
            level.onTextChange=[this,c]{auto& f=faders[static_cast<size_t>(c)];f.setValue(f.getValueFromText(levels[static_cast<size_t>(c)].getText()),juce::sendNotificationSync);};
            auto& b=buttons[static_cast<size_t>(c)];b=std::make_unique<MixerSwitch>(p,c,names[c]);b->setComponentID("mixer-power-"+juce::String(c));addAndMakeVisible(*b);
            buttonAttachments.push_back(std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter(p.mixerParameter(c,"Enabled")),*b));
            if(c<5) {
                auto& pan=pans[static_cast<size_t>(c)];pan.setSliderStyle(juce::Slider::LinearHorizontal);pan.getProperties().set("centre-fill",true);pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);pan.setPopupDisplayEnabled(true,true,nullptr);attach(pan,p.mixerParameter(c,"Pan"));
                knob(delaySends[static_cast<size_t>(c)]);attach(delaySends[static_cast<size_t>(c)],p.mixerParameter(c,"DelaySend"));delaySends[static_cast<size_t>(c)].setTooltip("Envio post-fader a Delay");
                knob(reverbSends[static_cast<size_t>(c)]);attach(reverbSends[static_cast<size_t>(c)],p.mixerParameter(c,"ReverbSend"));reverbSends[static_cast<size_t>(c)].setTooltip("Envio post-fader a Reverb");
            } else {knob(widths[static_cast<size_t>(c-5)]);widths[static_cast<size_t>(c-5)].getProperties().set("width-spread",true);attach(widths[static_cast<size_t>(c-5)],p.mixerParameter(c,"Width"));widths[static_cast<size_t>(c-5)].setTooltip("Width: 0 mono, 1 estereo original, 2 ampliado");}
        }
        tape.setComponentID("mixer-tape");tape.setClickingTogglesState(true);tape.setWantsKeyboardFocus(false);tape.getProperties().set("physical-switch",true);tape.getProperties().set("compact-switch",true);tape.setTooltip("Tape insertado en Master");addAndMakeVisible(tape);
        buttonAttachments.push_back(std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("tapeEnabled"),tape));timerCallback();startTimerHz(25);
    }
    void resized() override {
        for(int c=0;c<8;++c) {
            const int x=50+c*59;faders[static_cast<size_t>(c)].setBounds(x+7,86,33,88);levels[static_cast<size_t>(c)].setBounds(x-1,177,58,18);buttons[static_cast<size_t>(c)]->setBounds(x,198,56,27);
            if(c<5){delaySends[static_cast<size_t>(c)].setBounds(x+8,0,38,32);reverbSends[static_cast<size_t>(c)].setBounds(x+8,33,38,32);pans[static_cast<size_t>(c)].setBounds(x+1,67,53,17);}
            else widths[static_cast<size_t>(c-5)].setBounds(x+8,33,38,32);
        }
        tape.setBounds(50+7*59,2,56,27);
    }
    void paint(juce::Graphics& g) override {
        const auto ink=sampleui::palette(processor,"color-ff263e38"),accent=sampleui::palette(processor,"color-ffb66b44");
        g.setColour(ink);g.setFont(juce::FontOptions(10.f));
        g.drawText("Dly send",0,5,48,22,juce::Justification::centredLeft);g.drawText("Rev send",0,38,48,22,juce::Justification::centredLeft);g.drawText("Pan",0,65,48,20,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(8.5f));g.drawFittedText("Ctrl+clic\n= solo",{0,123,47,38},juce::Justification::centred,2);
        for(int c=0;c<8;++c) {
            const int x=50+c*59;
            if(c>=5){g.setColour(ink);g.setFont(juce::FontOptions(10.f));g.drawText("Width",x,65,56,20,juce::Justification::centred);}
            g.setColour(ink.withAlpha(.12f));g.fillRoundedRectangle(static_cast<float>(x+45),90,4,98,1.5f);
            const float height=std::clamp(peak[static_cast<size_t>(c)],0.f,1.f)*98;
            g.setColour(peak[static_cast<size_t>(c)]>.95f?juce::Colour(0xffe89a55):accent);g.fillRoundedRectangle(static_cast<float>(x+45),188-height,4,height,1.5f);
            if(c<7 && processor.state.getRawParameterValue(processor.mixerParameter(c,"Solo"))->load()>.5f){g.setColour(juce::Colour(0xfff0ca59));g.fillRoundedRectangle(static_cast<float>(x+3),195,50,2,1);}
        }
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours(){return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&){return {};}
private:
    void timerCallback() override {
        const auto ink=sampleui::palette(processor,"color-ff263e38"),accent=sampleui::palette(processor,"color-ffb66b44"),body=sampleui::palette(processor,"color-ffdad3bc");
        for(int i=0;i<getNumChildComponents();++i) {
            if(auto* s=dynamic_cast<juce::Slider*>(getChildComponent(i))){s->setColour(juce::Slider::thumbColourId,ink);s->setColour(juce::Slider::trackColourId,accent);s->setColour(juce::Slider::backgroundColourId,ink.withAlpha(.18f));s->setColour(juce::Slider::rotarySliderFillColourId,accent);}
            if(auto* b=dynamic_cast<juce::TextButton*>(getChildComponent(i))){b->setColour(juce::TextButton::buttonColourId,body);b->setColour(juce::TextButton::buttonOnColourId,accent);b->setColour(juce::TextButton::textColourOffId,ink);b->setColour(juce::TextButton::textColourOnId,ink.contrasting());}
        }
        for(size_t i=0;i<8;++i) {
            peak[i]=processor.mixerLevel(static_cast<int>(i));levels[i].setColour(juce::Label::textColourId,ink);
            if(!levels[i].isBeingEdited())levels[i].setText(faders[i].getTextFromValue(faders[i].getValue()),juce::dontSendNotification);
        }
        repaint();
    }
};
