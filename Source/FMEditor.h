#pragma once
#include "PluginProcessor.h"
#include "Themes.h"
#include "SampleRhythmEditor.h"

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

class FMCommonControls final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;bool fm=true;juce::ComboBox sound,octave,voices,type;
    std::array<juce::Slider,4> sliders;std::array<juce::Label,6> labels;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> combos;
public:
    explicit FMCommonControls(ChordHarpProcessor& p):processor(p) {
        for(auto* c:{&sound,&octave,&voices,&type})addAndMakeVisible(c);
        for(int i=1;i<=7;++i)voices.addItem(juce::String(i),i);octave.addItemList({"-1","0","+1"},1);type.addItemList({"LP","BP","HP"},1);
        const char* text[]{"COLOR","OCTAVA","VOCES","DETUNE","FILTER","RES"};
        for(size_t i=0;i<labels.size();++i){labels[i].setText(text[i],juce::dontSendNotification);labels[i].setFont(juce::FontOptions(9.f));addAndMakeVisible(labels[i]);}
        for(auto& slider:sliders){sampleui::slider(slider);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,42,20);addAndMakeVisible(slider);}
        voices.setComponentID("fm-voices");sliders[1].setComponentID("fm-detune");voices.setTooltip("Selecciona 1-7 voces. Para desafinar el conjunto elige 2 o mas.");
        sliders[1].setTooltip("Detune en cents. Con 1 voz no hay separacion; selecciona 2-7.");
        sound.onChange=[this]{if(sound.getSelectedId()>0 && sound.getSelectedId()<999)processor.selectSynthSound(fm,sound.getSelectedId()-1);};setBank(true);startTimerHz(10);
    }
    void setBank(bool isFM) {
        fm=isFM;attachments.clear();combos.clear();const juce::String prefix=fm?"fm":"harp";
        sound.clear(juce::dontSendNotification);sound.addItemList(processor.synthSoundNames(fm),1);sound.addItem("Personalizado",999);
        const juce::String ids[]{fm?juce::String("fmTone"):juce::String("tone"),prefix+"Detune",prefix+"Cutoff",prefix+"Resonance"};
        for(size_t i=0;i<4;++i)attachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*processor.state.getParameter(ids[i]),sliders[i]));
        combos.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*processor.state.getParameter(prefix+"Unison"),voices));
        combos.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*processor.state.getParameter(prefix+"FilterType"),type));
        octave.setEnabled(fm);if(fm)combos.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*processor.state.getParameter("fmOctave"),octave));else octave.setText("--",juce::dontSendNotification);
        timerCallback();
    }
    void resized() override {
        sound.setBounds(0,0,180,22);labels[0].setBounds(185,0,43,22);sliders[0].setBounds(228,0,125,22);labels[1].setBounds(363,0,57,22);octave.setBounds(422,0,103,22);
        labels[2].setBounds(0,29,40,22);voices.setBounds(42,29,45,22);labels[3].setBounds(95,29,45,22);sliders[1].setBounds(140,29,70,22);
        labels[4].setBounds(218,29,42,22);sliders[2].setBounds(260,29,100,22);labels[5].setBounds(368,29,29,22);sliders[3].setBounds(397,29,60,22);type.setBounds(465,29,60,22);
    }
private:
    void timerCallback() override {const int selected=processor.getSynthChoice(fm);sound.setSelectedId(selected<0?999:selected+1,juce::dontSendNotification);sampleui::colours(processor,*this);}
};

class FMPanel final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;juce::ComboBox target,op,algorithm;juce::TextButton enabled{"FM"};juce::Label description;
    std::array<juce::Slider,11> sliders;std::array<juce::Label,11> labels;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;
    std::unique_ptr<juce::ComboBoxParameterAttachment> algorithmAttachment;
    bool organ=true;FMCommonControls common;
public:
    explicit FMPanel(ChordHarpProcessor& p):processor(p),common(p) {
        target.addItem("HARP",1);target.addItem("FM",2);target.setSelectedId(2,juce::dontSendNotification);
        for(int i=1;i<=6;++i)op.addItem("OP "+juce::String(i),i);op.setSelectedId(1,juce::dontSendNotification);
        for(int i=0;i<32;++i)algorithm.addItem(juce::String(i+1)+"  ("+juce::String(chordharp::fmCarriers(i))+" portadoras)",i+1);
        target.setComponentID("fm-target");op.setComponentID("fm-operator");algorithm.setComponentID("fm-algorithm");enabled.setComponentID("fm-enabled");
        for(auto* component:std::array<juce::Component*,5>{{&target,&op,&algorithm,&enabled,&description}})addAndMakeVisible(component);
        enabled.setClickingTogglesState(true);enabled.onClick=[this]{auto* p=processor.state.getParameter(organ?"fmEnabled":"harpSound");p->setValueNotifyingHost(enabled.getToggleState()?1.f:0.f);};
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
        addAndMakeVisible(common);description.setFont(juce::FontOptions(10.f));bind();startTimerHz(10);
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
        description.setBounds(0,154,525,17);common.setBounds(0,174,525,51);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() {return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) {return {};}
private:
    void bind() {
        common.setBank(organ);algorithmAttachment.reset();attachments.clear();const juce::String prefix=organ?"fm":"harp";
        constexpr const char* field[]{"Ratio","Fine","Level","Velocity","Attack","Decay","Sustain","Release"};
        for(size_t i=0;i<8;++i)attachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*processor.state.getParameter(ChordHarpProcessor::fmOperatorID(organ,op.getSelectedId()-1,field[i])),sliders[i]));
        const char* common[]{"Feedback","Width","Drift"};
        for(size_t i=0;i<3;++i)attachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*processor.state.getParameter(i==0 && organ?juce::String("organFeedback"):prefix+common[i]),sliders[8+i]));
        algorithmAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*processor.state.getParameter(organ?juce::String("organAlgorithm"):prefix+"Algorithm"),algorithm);timerCallback();
    }
    void timerCallback() override {
        const auto palette=processor.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
        const auto colour=[&](const char* key,juce::uint32 fallback){return juce::Colour::fromString(palette.getProperty(key,juce::Colour(fallback).toString()).toString());};
        const auto ink=colour("color-ff263e38",0xff263e38),accent=colour("color-ffb66b44",0xffb66b44),button=colour("color-ffdad3bc",0xffdad3bc);
        for(auto& slider:sliders) {slider.setColour(juce::Slider::thumbColourId,accent);slider.setColour(juce::Slider::trackColourId,accent);slider.setColour(juce::Slider::textBoxTextColourId,ink);slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);}
        for(auto& label:labels)label.setColour(juce::Label::textColourId,ink);description.setColour(juce::Label::textColourId,ink);
        for(auto* combo:{&target,&op,&algorithm}) {combo->setColour(juce::ComboBox::backgroundColourId,button);combo->setColour(juce::ComboBox::textColourId,ink);}
        const bool fm=processor.state.getRawParameterValue(organ?"fmEnabled":"harpSound")->load()>.5f;enabled.setToggleState(fm,juce::dontSendNotification);
        enabled.setColour(juce::TextButton::buttonColourId,button);enabled.setColour(juce::TextButton::buttonOnColourId,accent);
        const int a=static_cast<int>(processor.state.getRawParameterValue((organ?"organ":"harp")+juce::String("Algorithm"))->load())-1;
        juce::String carriers;for(int i=0;i<6;++i)if(chordharp::fmCarrier(a,i))carriers+=juce::String(i+1)+" ";
        description.setText("Portadoras: "+carriers+" | OP "+juce::String(op.getSelectedId())+(chordharp::fmCarrier(a,op.getSelectedId()-1)?" es portadora":" es moduladora"),juce::dontSendNotification);
    }
};
