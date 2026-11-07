#pragma once
#include "PluginProcessor.h"
#include "Themes.h"

namespace sampleui {
inline void import(ChordHarpProcessor& processor,juce::Component& owner,std::unique_ptr<juce::FileChooser>& chooser) {
    chooser=std::make_unique<juce::FileChooser>("Cargar libreria de samples ZIP",juce::File{},"*.zip");
    juce::Component::SafePointer<juce::Component> safe(&owner);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,&processor](const juce::FileChooser& selected){
        if(!safe || selected.getResult()==juce::File{})return;
        juce::String error;
        if(!processor.loadSampleLibrary(selected.getResult(),error))juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Samples",error);
    });
}
inline juce::Colour palette(ChordHarpProcessor& p,const char* key) {return juce::Colour::fromString(p.magicState.getGuiTree().getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0).getProperty(key).toString());}
inline void slider(juce::Slider& s) {s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,48,20);}
inline void colours(ChordHarpProcessor& p,juce::Component& owner) {
    const auto ink=palette(p,"color-ff263e38"),accent=palette(p,"color-ffb66b44"),button=palette(p,"color-ffdad3bc");
    for(int i=0;i<owner.getNumChildComponents();++i) {
        auto* child=owner.getChildComponent(i);
        if(auto* label=dynamic_cast<juce::Label*>(child))label->setColour(juce::Label::textColourId,ink);
        if(auto* b=dynamic_cast<juce::TextButton*>(child)) {b->setColour(juce::TextButton::buttonColourId,button);b->setColour(juce::TextButton::buttonOnColourId,accent);b->setColour(juce::TextButton::textColourOffId,ink);b->setColour(juce::TextButton::textColourOnId,ink.contrasting());}
        if(auto* c=dynamic_cast<juce::ComboBox*>(child)){c->setColour(juce::ComboBox::backgroundColourId,button);c->setColour(juce::ComboBox::textColourId,ink);}
        if(auto* s=dynamic_cast<juce::Slider*>(child)){s->setColour(juce::Slider::thumbColourId,accent);s->setColour(juce::Slider::trackColourId,accent);s->setColour(juce::Slider::textBoxTextColourId,ink);s->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);s->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);}
    }
}
}
class SamplePanel final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;juce::TextButton load{"CARGAR ZIP"},activate{"SAMPLES"},loop{"LOOP"},whole{"ACORDE GRABADO"};
    juce::ComboBox sound;juce::Slider root,release;juce::Label status,rootLabel,releaseLabel,help;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::SliderParameterAttachment> rootAttachment,releaseAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> loopAttachment,wholeAttachment;
    unsigned revision=~0u;
public:
    explicit SamplePanel(ChordHarpProcessor& p):processor(p) {
        for(auto* c:std::array<juce::Component*,12>{{&load,&activate,&loop,&whole,&sound,&root,&release,&status,&rootLabel,&releaseLabel,&help,nullptr}})if(c)addAndMakeVisible(c);
        for(auto* b:{&activate,&loop,&whole}){b->setClickingTogglesState(true);b->getProperties().set("physical-switch",true);}
        load.onClick=[this]{sampleui::import(processor,*this,chooser);};
        activate.onClick=[this]{auto* p=processor.state.getParameter("organModel");p->setValueNotifyingHost(p->convertTo0to1(activate.getToggleState()?4.f:0.f));};
        sound.setComponentID("sample-sound");sound.onChange=[this]{if(sound.getSelectedId()>0){auto* p=processor.state.getParameter("organSample");p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(sound.getSelectedId()-1)));}};
        sampleui::slider(root);sampleui::slider(release);root.setComponentID("sample-root");release.setComponentID("sample-release");
        rootAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("sampleRoot"),root);
        releaseAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("sampleRelease"),release);
        loopAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("sampleLoop"),loop);
        wholeAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("sampleChord"),whole);
        rootLabel.setText("NOTA RAIZ MIDI",juce::dontSendNotification);releaseLabel.setText("RELEASE ms",juce::dontSendNotification);
        help.setFont(juce::FontOptions(10.f));help.setText("Nota aislada: crea Maj, min, 7 y Maj7. Acorde grabado: conserva sus intervalos.\nEl volumen, la octava y el filtro se ajustan en Organo. Activa ORGAN para tocar.",juce::dontSendNotification);
        whole.setTooltip("Para una grabacion de un acorde completo: solo se transpone la raiz. Su calidad grabada no cambia al seleccionar otra fila.");
        loop.setTooltip("Loop automatico con crossfade en la parte central. Puede no servir para todas las grabaciones.");
        timerCallback();startTimerHz(10);
    }
    void resized() override {
        load.setBounds(0,2,135,24);status.setBounds(145,2,380,24);sound.setBounds(0,40,395,24);activate.setBounds(405,40,120,26);
        rootLabel.setBounds(0,78,105,24);root.setBounds(110,78,145,24);releaseLabel.setBounds(275,78,85,24);release.setBounds(365,78,160,24);
        loop.setBounds(0,115,120,26);whole.setBounds(135,115,205,26);help.setBounds(0,150,525,32);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours(){return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&){return {};}
private:
    void timerCallback() override {
        if(revision!=processor.getSampleRevision()) {revision=processor.getSampleRevision();const auto names=processor.sampleNames();sound.clear(juce::dontSendNotification);sound.addItemList(names,1);status.setText(names.isEmpty()?"Carga un ZIP con tus samples":juce::String(names.size())+" samples disponibles",juce::dontSendNotification);}
        sound.setSelectedId(static_cast<int>(processor.state.getRawParameterValue("organSample")->load())+1,juce::dontSendNotification);
        activate.setToggleState(processor.state.getRawParameterValue("organModel")->load()==4,juce::dontSendNotification);sampleui::colours(processor,*this);
    }
};
class RhythmPanel final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;juce::TextButton play{"PLAY"},follow{"DAW"},load{"ZIP"};juce::ComboBox pattern;
    juce::Slider tempo,swing,volume,send;juce::Label bpmLabel,swingLabel,volumeLabel,sendLabel;
    std::array<juce::ComboBox,5> samples;std::array<std::array<juce::TextButton,16>,5> steps;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliders;
    std::unique_ptr<juce::ButtonParameterAttachment> playAttachment,followAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> patternAttachment;
    std::unique_ptr<juce::FileChooser> chooser;unsigned revision=~0u;
public:
    explicit RhythmPanel(ChordHarpProcessor& p):processor(p) {
        for(auto* c:std::array<juce::Component*,13>{{&play,&follow,&load,&pattern,&tempo,&swing,&volume,&send,&bpmLabel,&swingLabel,&volumeLabel,&sendLabel,nullptr}})if(c)addAndMakeVisible(c);
        play.setComponentID("rhythm-play");follow.setComponentID("rhythm-follow");pattern.setComponentID("rhythm-pattern");
        for(auto* button:{&play,&follow}){button->setClickingTogglesState(true);button->getProperties().set("physical-switch",true);}
        playAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("rhythmEnabled"),play);followAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("rhythmHost"),follow);
        for(int i=0;i<7;++i)pattern.addItem(chordharp::rhythmNames[static_cast<size_t>(i)],i+1);
        patternAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*p.state.getParameter("rhythmPattern"),pattern);
        const char* ids[]{"delayTempo","rhythmSwing","rhythmVolume","rhythmSend"};juce::Slider* controls[]{&tempo,&swing,&volume,&send};
        for(size_t i=0;i<4;++i){sampleui::slider(*controls[i]);sliders.push_back(std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter(ids[i]),*controls[i]));}
        bpmLabel.setText("BPM",juce::dontSendNotification);swingLabel.setText("SWING",juce::dontSendNotification);volumeLabel.setText("VOL",juce::dontSendNotification);sendLabel.setText("FX",juce::dontSendNotification);
        load.onClick=[this]{sampleui::import(processor,*this,chooser);};
        for(size_t t=0;t<5;++t) {
            addAndMakeVisible(samples[t]);samples[t].setComponentID("rhythm-sample-"+juce::String(static_cast<int>(t)));
            samples[t].onChange=[this,t]{if(samples[t].getSelectedId()>0){auto* p=processor.state.getParameter("rhythmSample"+juce::String(static_cast<int>(t)));p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(samples[t].getSelectedId()-1)));}};
            for(size_t s=0;s<16;++s) {
                auto& b=steps[t][s];addAndMakeVisible(b);b.setClickingTogglesState(true);b.setComponentID(ChordHarpProcessor::rhythmStepID(static_cast<int>(t),static_cast<int>(s)));
                b.setButtonText(juce::String(static_cast<int>(s+1)));b.onClick=[this,t,s]{processor.editRhythmStep(static_cast<int>(t),static_cast<int>(s),steps[t][s].getToggleState());};
            }
        }
        tempo.setTooltip("BPM de standalone. Si el DAW aporta tempo se utiliza el del DAW.");follow.setTooltip("Sigue la posicion y play/stop del DAW. Desactivado: el patron corre libre al BPM actual.");
        timerCallback();startTimerHz(20);
    }
    void resized() override {
        play.setBounds(0,2,55,24);follow.setBounds(62,2,50,24);pattern.setBounds(120,2,125,24);
        bpmLabel.setBounds(250,2,30,24);tempo.setBounds(280,2,100,24);swingLabel.setBounds(390,2,45,24);swing.setBounds(435,2,90,24);
        load.setBounds(0,31,55,23);volumeLabel.setBounds(120,31,30,23);volume.setBounds(155,31,150,23);sendLabel.setBounds(330,31,25,23);send.setBounds(360,31,165,23);
        for(int t=0;t<5;++t){const int y=60+t*24;samples[static_cast<size_t>(t)].setBounds(0,y,110,22);for(int s=0;s<16;++s)steps[static_cast<size_t>(t)][static_cast<size_t>(s)].setBounds(117+s*25,y,22,22);}
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours(){return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&){return {};}
private:
    void timerCallback() override {
        if(revision!=processor.getSampleRevision()) {
            revision=processor.getSampleRevision();const auto names=processor.sampleNames();const char* tracks[]{"KICK","SNARE","C.HAT","O.HAT","PERC"};
            for(size_t t=0;t<5;++t){samples[t].clear(juce::dontSendNotification);samples[t].addItem(juce::String(tracks[t])+": Auto",1);samples[t].addItemList(names,2);}
            load.setTooltip(names.isEmpty()?"Carga un ZIP con samples para escuchar la bateria.":juce::String(names.size())+" samples cargados. Puedes asignar cada pista.");
        }
        const int selected=static_cast<int>(processor.state.getRawParameterValue("rhythmPattern")->load()),current=processor.rhythmStep();const bool waltz=selected==5;
        const auto accent=sampleui::palette(processor,"color-ffb66b44"),ink=sampleui::palette(processor,"color-ff263e38"),button=sampleui::palette(processor,"color-ffdad3bc");
        sampleui::colours(processor,*this);tempo.setEnabled(!processor.tempoComesFromHost());
        for(size_t t=0;t<5;++t) {
            samples[t].setSelectedId(static_cast<int>(processor.state.getRawParameterValue("rhythmSample"+juce::String(static_cast<int>(t)))->load())+1,juce::dontSendNotification);
            for(size_t s=0;s<16;++s) {
                auto& b=steps[t][s];const bool on=selected<6?(chordharp::rhythmPatterns[static_cast<size_t>(selected)][t]&(1u<<s))!=0:processor.state.getRawParameterValue(ChordHarpProcessor::rhythmStepID(static_cast<int>(t),static_cast<int>(s)))->load()>.5f;
                b.setToggleState(on,juce::dontSendNotification);b.setEnabled(!waltz || s<12);
                b.setColour(juce::TextButton::buttonColourId,static_cast<int>(s)==current?button.brighter(.5f):button);b.setColour(juce::TextButton::buttonOnColourId,static_cast<int>(s)==current?accent.brighter(.4f):accent);b.setColour(juce::TextButton::textColourOffId,ink);b.setColour(juce::TextButton::textColourOnId,ink.contrasting());
            }
        }
    }
};
