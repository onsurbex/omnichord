#pragma once
#include "PluginProcessor.h"
#include "Themes.h"

class SynthSelector final : public juce::ComboBox,private juce::Timer {
    ChordHarpProcessor& processor;bool organ=true;unsigned revision=~0u;
public:
    explicit SynthSelector(ChordHarpProcessor& p):processor(p) {onChange=[this]{if(getSelectedId()>0 && getSelectedId()<999)processor.selectSynthSound(organ,getSelectedId()-1);};startTimerHz(10);}
    void configure(foleys::GuiItem& item) {
        organ=item.getProperty("target").toString()!="harp";clear(juce::dontSendNotification);
        rebuild();timerCallback();
    }
    static std::vector<std::pair<juce::String,int>> colours() {return {{"combo-background",juce::ComboBox::backgroundColourId},{"combo-text",juce::ComboBox::textColourId},{"combo-outline",juce::ComboBox::outlineColourId}};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) {return {};}
private:
    void rebuild() {
        clear(juce::dontSendNotification);addItemList(processor.synthSoundNames(organ),1);addSeparator();addItem("FM personalizado",999);revision=processor.getSampleRevision();
    }
    void timerCallback() override {if(revision!=processor.getSampleRevision())rebuild();const int choice=processor.getSynthChoice(organ);setSelectedId(choice<0?999:choice+1,juce::dontSendNotification);}
};

class FMPanel final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;juce::ComboBox target,op,algorithm;juce::TextButton enabled{"FM"};juce::Label description;
    std::array<juce::Slider,11> sliders;std::array<juce::Label,11> labels;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::ComboBoxParameterAttachment> algorithmAttachment;
    bool organ=false;
public:
    explicit FMPanel(ChordHarpProcessor& p):processor(p) {
        target.addItem("HARP",1);target.addItem("ORGAN",2);target.setSelectedId(1,juce::dontSendNotification);
        for(int i=1;i<=6;++i)op.addItem("OP "+juce::String(i),i);op.setSelectedId(1,juce::dontSendNotification);
        for(int i=0;i<32;++i)algorithm.addItem(juce::String(i+1)+"  ("+juce::String(chordharp::fmCarriers(i))+" portadoras)",i+1);
        target.setComponentID("fm-target");op.setComponentID("fm-operator");algorithm.setComponentID("fm-algorithm");enabled.setComponentID("fm-enabled");
        for(auto* component:std::array<juce::Component*,5>{{&target,&op,&algorithm,&enabled,&description}})addAndMakeVisible(component);
        enabled.setClickingTogglesState(true);enabled.onClick=[this]{auto* p=processor.state.getParameter(organ?"organModel":"harpSound");p->setValueNotifyingHost(p->convertTo0to1(enabled.getToggleState()?(organ?3.f:1.f):0.f));};
        target.onChange=[this]{organ=target.getSelectedId()==2;bind();};op.onChange=[this]{bind();};
        constexpr const char* text[]{"RATIO","FINE","LEVEL","VEL","ATK ms","DEC ms","SUST","REL ms","FDBK","WIDTH","DRIFT"};
        constexpr const char* ids[]{"fm-ratio","fm-fine","fm-level","fm-velocity","fm-attack","fm-decay","fm-sustain","fm-release","fm-feedback","fm-width","fm-drift"};
        for(size_t i=0;i<sliders.size();++i) {
            auto& slider=sliders[i];slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,48,20);slider.setComponentID(ids[i]);
            addAndMakeVisible(slider);labels[i].setText(text[i],juce::dontSendNotification);labels[i].setFont(juce::FontOptions(10.f));addAndMakeVisible(labels[i]);
        }
        sliders[0].setTooltip("Relacion de frecuencia del operador respecto a la nota; admite ratios fraccionarios.");
        sliders[1].setTooltip("Afinacion fina de este operador, en cents.");sliders[3].setTooltip("Sensibilidad de este operador a la velocidad MIDI.");
        sliders[10].setTooltip("Deriva lenta e independiente de las voces del unison.");
        description.setFont(juce::FontOptions(10.f));bind();startTimerHz(10);
    }
    void resized() override {
        target.setBounds(0,2,85,22);enabled.setBounds(92,2,38,22);algorithm.setBounds(138,2,155,22);
        labels[8].setBounds(300,2,54,22);sliders[8].setBounds(355,2,170,22);
        op.setBounds(0,34,65,22);
        labels[0].setBounds(72,34,37,22);sliders[0].setBounds(109,34,103,22);
        labels[1].setBounds(220,34,32,22);sliders[1].setBounds(252,34,104,22);
        labels[2].setBounds(365,34,38,22);sliders[2].setBounds(403,34,122,22);
        for(int row=0;row<2;++row) {
            const size_t a=static_cast<size_t>(4+row*2),b=a+1;const int y=66+32*row;
            labels[a].setBounds(0,y,50,22);sliders[a].setBounds(55,y,190,22);labels[b].setBounds(270,y,50,22);sliders[b].setBounds(325,y,200,22);
        }
        labels[3].setBounds(0,130,35,22);sliders[3].setBounds(35,130,130,22);
        labels[9].setBounds(175,130,45,22);sliders[9].setBounds(220,130,120,22);
        labels[10].setBounds(350,130,43,22);sliders[10].setBounds(393,130,132,22);
        description.setBounds(0,162,525,18);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() {return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) {return {};}
private:
    void bind() {
        algorithmAttachment.reset();attachments.clear();const juce::String prefix=organ?"organ":"harp";
        constexpr const char* field[]{"Ratio","Fine","Level","Velocity","Attack","Decay","Sustain","Release"};
        for(size_t i=0;i<8;++i)attachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*processor.state.getParameter(ChordHarpProcessor::fmOperatorID(organ,op.getSelectedId()-1,field[i])),sliders[i]));
        const char* common[]{"Feedback","Width","Drift"};
        for(size_t i=0;i<3;++i)attachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*processor.state.getParameter(prefix+common[i]),sliders[8+i]));
        algorithmAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*processor.state.getParameter(prefix+"Algorithm"),algorithm);timerCallback();
    }
    void timerCallback() override {
        const auto palette=processor.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
        const auto colour=[&](const char* key,juce::uint32 fallback){return juce::Colour::fromString(palette.getProperty(key,juce::Colour(fallback).toString()).toString());};
        const auto ink=colour("color-ff263e38",0xff263e38),accent=colour("color-ffb66b44",0xffb66b44),button=colour("color-ffdad3bc",0xffdad3bc);
        for(auto& slider:sliders) {slider.setColour(juce::Slider::thumbColourId,accent);slider.setColour(juce::Slider::trackColourId,accent);slider.setColour(juce::Slider::textBoxTextColourId,ink);slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);}
        for(auto& label:labels)label.setColour(juce::Label::textColourId,ink);description.setColour(juce::Label::textColourId,ink);
        for(auto* combo:{&target,&op,&algorithm}) {combo->setColour(juce::ComboBox::backgroundColourId,button);combo->setColour(juce::ComboBox::textColourId,ink);}
        const bool fm=processor.state.getRawParameterValue(organ?"organModel":"harpSound")->load()==(organ?3.f:1.f);enabled.setToggleState(fm,juce::dontSendNotification);
        enabled.setColour(juce::TextButton::buttonColourId,button);enabled.setColour(juce::TextButton::buttonOnColourId,accent);
        const int a=static_cast<int>(processor.state.getRawParameterValue((organ?"organ":"harp")+juce::String("Algorithm"))->load())-1;
        juce::String carriers;for(int i=0;i<6;++i)if(chordharp::fmCarrier(a,i))carriers+=juce::String(i+1)+" ";
        description.setText("Portadoras: "+carriers+" | OP "+juce::String(op.getSelectedId())+(chordharp::fmCarrier(a,op.getSelectedId()-1)?" es portadora":" es moduladora"),juce::dontSendNotification);
    }
};
