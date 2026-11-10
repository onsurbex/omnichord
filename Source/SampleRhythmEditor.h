#pragma once
#include "PluginProcessor.h"
#include "Themes.h"
#include <map>

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
    juce::ComboBox sound,octave,type;juce::Slider root,release,cutoff,resonance;juce::Label cutoffLabel,resonanceLabel;juce::Label status,rootLabel,releaseLabel,help;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::SliderParameterAttachment> rootAttachment,releaseAttachment,cutoffAttachment,resonanceAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> octaveAttachment,typeAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> loopAttachment,wholeAttachment;
    unsigned revision=~0u;
public:
    explicit SamplePanel(ChordHarpProcessor& p):processor(p) {
        for(auto* c:std::array<juce::Component*,12>{{&load,&activate,&loop,&whole,&sound,&root,&release,&status,&rootLabel,&releaseLabel,&help,nullptr}})if(c)addAndMakeVisible(c);
        for(auto* b:{&activate,&loop,&whole}){b->setClickingTogglesState(true);b->getProperties().set("physical-switch",true);}
        load.onClick=[this]{sampleui::import(processor,*this,chooser);};
        activate.onClick=[this]{auto* p=processor.state.getParameter("samplesEnabled");p->setValueNotifyingHost(activate.getToggleState()?1.f:0.f);};
        sound.setComponentID("sample-sound");sound.onChange=[this]{if(sound.getSelectedId()>0)processor.selectSampleSound(sound.getSelectedId()-1);};
        sampleui::slider(root);sampleui::slider(release);root.setComponentID("sample-root");release.setComponentID("sample-release");
        rootAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("sampleRoot"),root);
        releaseAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("sampleRelease"),release);
        loopAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("sampleLoop"),loop);
        wholeAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("sampleChord"),whole);
        rootLabel.setText("NOTA RAIZ MIDI",juce::dontSendNotification);releaseLabel.setText("RELEASE ms",juce::dontSendNotification);
        help.setFont(juce::FontOptions(10.f));help.setText("Nota aislada: crea Maj, min, 7 y Maj7. Acorde grabado: conserva sus intervalos.\nVolumen en Mixer. La octava y el filtro se ajustan aqui. Activa CHORDS para tocar.",juce::dontSendNotification);
        whole.setTooltip("Para una grabacion de un acorde completo: solo se transpone la raiz. Su calidad grabada no cambia al seleccionar otra fila.");
        loop.setTooltip("Loop automatico con crossfade en la parte central. Puede no servir para todas las grabaciones.");
        for(auto* c:std::array<juce::Component*,6>{{&octave,&type,&cutoff,&resonance,&cutoffLabel,&resonanceLabel}})addAndMakeVisible(c);
        octave.addItemList({"-1","0","+1"},1);type.addItemList({"LP","BP","HP"},1);sampleui::slider(cutoff);sampleui::slider(resonance);
        cutoffLabel.setText("FILTER Hz",juce::dontSendNotification);resonanceLabel.setText("RES",juce::dontSendNotification);
        cutoffAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("sampleCutoff"),cutoff);
        resonanceAttachment=std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter("sampleResonance"),resonance);
        octaveAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*p.state.getParameter("sampleOctave"),octave);
        typeAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*p.state.getParameter("sampleFilterType"),type);
        timerCallback();startTimerHz(10);
    }
    void resized() override {
        load.setBounds(0,2,135,24);status.setBounds(145,2,380,24);sound.setBounds(0,40,395,24);activate.setBounds(405,40,120,26);
        rootLabel.setBounds(0,78,105,24);root.setBounds(110,78,145,24);releaseLabel.setBounds(275,78,85,24);release.setBounds(365,78,160,24);
        loop.setBounds(0,115,120,26);whole.setBounds(135,115,205,26);help.setBounds(0,150,525,32);octave.setBounds(350,115,175,26);cutoffLabel.setBounds(0,195,75,22);cutoff.setBounds(80,195,180,22);resonanceLabel.setBounds(275,195,35,22);resonance.setBounds(310,195,110,22);type.setBounds(435,195,90,22);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours(){return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&){return {};}
private:
    void timerCallback() override {
        if(revision!=processor.getSampleRevision()) {
            revision=processor.getSampleRevision();const auto choices=processor.sampleChoices();sound.clear(juce::dontSendNotification);
            std::map<juce::String,juce::PopupMenu> groups;
            for(const auto& item:choices) {
                if(item.second.startsWith("GraySlate / ")) {
                    const auto group=item.second.fromFirstOccurrenceOf(" / ",false,false).upToFirstOccurrenceOf(" / ",false,false);
                    groups[group].addItem(item.first+1,item.second);
                } else sound.addItem(item.second,item.first+1);
            }
            if(!groups.empty()){juce::PopupMenu bank;for(auto& group:groups)bank.addSubMenu(group.first,group.second);sound.getRootMenu()->addSubMenu("GraySlate Omnichord",bank);}
            status.setText(choices.empty()?"Carga un ZIP con tus instrumentos":juce::String(static_cast<int>(choices.size()))+" instrumentos disponibles",juce::dontSendNotification);
        }
        sound.setSelectedId(static_cast<int>(processor.state.getRawParameterValue("organSample")->load())+1,juce::dontSendNotification);
        activate.setToggleState(processor.state.getRawParameterValue("samplesEnabled")->load()>.5f,juce::dontSendNotification);sampleui::colours(processor,*this);
    }
};
class PatternRuler final : public juce::Component,public juce::SettableTooltipClient {
    ChordHarpProcessor& processor;bool resizing=false;float anchor=0;int originalLength=16;
    int edge() const {return processor.rhythmPatternLength()*25-2;}
public:
    explicit PatternRuler(ChordHarpProcessor& p):processor(p) {
        setComponentID("rhythm-ruler");setWantsKeyboardFocus(false);setTooltip("Arrastra el tirador derecho para elegir 1-16 pasos. Los pasos ocultos se conservan.");
    }
    void paint(juce::Graphics& g) override {
        const auto ink=sampleui::palette(processor,"color-ff263e38"),accent=sampleui::palette(processor,"color-ffb66b44"),button=sampleui::palette(processor,"color-ffdad3bc");
        const int length=processor.rhythmPatternLength(),current=processor.rhythmStep();g.setFont(juce::FontOptions(10.f));
        for(int step=0;step<length;++step) {
            const bool active=step==current;auto cell=juce::Rectangle<float>(static_cast<float>(step*25),1,22,20);
            g.setColour(active?accent:((step/2)%2?button.darker(.18f):button));g.fillRoundedRectangle(cell,2);
            g.setColour(ink.withAlpha(.2f));g.drawRoundedRectangle(cell,2,.7f);
            g.setColour(active?accent.contrasting():ink);g.drawText(juce::String(step+1),cell.toNearestInt(),juce::Justification::centred);
        }
        const float x=static_cast<float>(edge());g.setColour(resizing?accent:ink.withAlpha(.7f));
        g.fillRoundedRectangle(x-2,0,7,22,2);g.setColour(ink.contrasting(.8f));g.drawLine(x,5,x,17,1);g.drawLine(x+2,5,x+2,17,1);
    }
    void mouseMove(const juce::MouseEvent& e) override {setMouseCursor(std::abs(e.x-edge())<=8?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);}
    void mouseDown(const juce::MouseEvent& e) override {
        if(std::abs(e.x-edge())>8)return;resizing=true;anchor=e.position.x;originalLength=processor.rhythmPatternLength();
        processor.state.getParameter("rhythmLength")->beginChangeGesture();repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(resizing){processor.resizeRhythmPattern(originalLength+juce::roundToInt((e.position.x-anchor)/25));repaint();}
    }
    void mouseUp(const juce::MouseEvent&) override {
        if(resizing){resizing=false;processor.state.getParameter("rhythmLength")->endChangeGesture();repaint();}
    }
};

class RhythmPanel final : public juce::Component,private juce::Timer {
    ChordHarpProcessor& processor;PatternRuler ruler;juce::TextButton play{"PLAY"},follow{"DAW"},load{"ZIP"},loopMode{"LOOP"};juce::ComboBox pattern;
    juce::Slider tempo,swing,volume,send;juce::Label bpmLabel,swingLabel,volumeLabel,sendLabel,loopNotice,transport;
    std::array<juce::ComboBox,5> samples;std::array<std::array<juce::TextButton,16>,5> steps;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliders;
    std::unique_ptr<juce::ButtonParameterAttachment> playAttachment,followAttachment,modeAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> patternAttachment;
    std::unique_ptr<juce::FileChooser> chooser;unsigned revision=~0u;bool shownLoop=false;
public:
    explicit RhythmPanel(ChordHarpProcessor& p):processor(p),ruler(p) {
        addAndMakeVisible(ruler);
        for(auto* c:std::array<juce::Component*,14>{{&play,&follow,&load,&loopMode,&pattern,&tempo,&swing,&volume,&send,&bpmLabel,&swingLabel,&volumeLabel,&sendLabel,&loopNotice}})addAndMakeVisible(c);
        transport.setComponentID("rhythm-transport");transport.setFont(juce::FontOptions(10.f));addAndMakeVisible(transport);
        play.setComponentID("rhythm-play");follow.setComponentID("rhythm-follow");pattern.setComponentID("rhythm-pattern");
        for(auto* button:{&play,&follow,&loopMode}){button->setClickingTogglesState(true);button->getProperties().set("physical-switch",true);}
        playAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("rhythmEnabled"),play);followAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("rhythmHost"),follow);
        modeAttachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter("rhythmUseLoop"),loopMode);
        loopMode.setComponentID("rhythm-loop-mode");loopNotice.setFont(juce::FontOptions(12.f));loopNotice.setJustificationType(juce::Justification::centred);
        loopMode.setTooltip("Activa los loops grabados; desactivado utiliza los golpes y pasos editables. Los loops se adaptan al BPM conservando su afinacion.");
        const char* ids[]{"delayTempo","rhythmSwing","rhythmVolume","rhythmReverbSend"};juce::Slider* controls[]{&tempo,&swing,&volume,&send};
        for(size_t i=0;i<4;++i){sampleui::slider(*controls[i]);sliders.push_back(std::make_unique<juce::SliderParameterAttachment>(*p.state.getParameter(ids[i]),*controls[i]));}
        bpmLabel.setText("BPM",juce::dontSendNotification);swingLabel.setText("SWING",juce::dontSendNotification);volumeLabel.setText("VOL",juce::dontSendNotification);sendLabel.setText("REV",juce::dontSendNotification);
        load.onClick=[this]{sampleui::import(processor,*this,chooser);};
        for(size_t t=0;t<5;++t) {
            addAndMakeVisible(samples[t]);samples[t].setComponentID("rhythm-sample-"+juce::String(static_cast<int>(t)));
            samples[t].onChange=[this,t]{if(samples[t].getSelectedId()>0){auto* p=processor.state.getParameter("rhythmSample"+juce::String(static_cast<int>(t)));p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(samples[t].getSelectedId()-1)));}};
            for(size_t s=0;s<16;++s) {
                auto& b=steps[t][s];addAndMakeVisible(b);b.setClickingTogglesState(true);b.setComponentID(ChordHarpProcessor::rhythmStepID(static_cast<int>(t),static_cast<int>(s)));
                b.setButtonText({});b.setWantsKeyboardFocus(false);b.onClick=[this,t,s]{processor.editRhythmStep(static_cast<int>(t),static_cast<int>(s),steps[t][s].getToggleState());};
            }
        }
        tempo.setTooltip("BPM de standalone. Si el DAW aporta tempo se utiliza el del DAW.");follow.setTooltip("Sigue la posicion y play/stop del DAW. Desactivado: el patron corre libre al BPM actual.");
        timerCallback();startTimerHz(20);
    }
    void resized() override {
        play.setBounds(0,2,55,24);follow.setBounds(62,2,50,24);pattern.setBounds(120,2,125,24);
        bpmLabel.setBounds(250,2,30,24);tempo.setBounds(280,2,100,24);swingLabel.setBounds(390,2,45,24);swing.setBounds(435,2,90,24);
        load.setBounds(0,31,50,23);loopMode.setBounds(57,31,58,23);volumeLabel.setBounds(120,31,30,23);volume.setBounds(155,31,150,23);sendLabel.setBounds(330,31,25,23);send.setBounds(360,31,165,23);
        for(int t=0;t<5;++t){const int y=60+t*24;samples[static_cast<size_t>(t)].setBounds(0,y,110,22);for(int s=0;s<16;++s)steps[static_cast<size_t>(t)][static_cast<size_t>(s)].setBounds(117+s*25,y,22,22);}
        loopNotice.setBounds(0,60,525,118);ruler.setBounds(117,181,408,22);transport.setBounds(0,204,525,22);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours(){return {};}
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&){return {};}
private:
    void timerCallback() override {
        const bool looping=processor.state.getRawParameterValue("rhythmUseLoop")->load()>.5f;
        const bool host=processor.rhythmFollowsTransport(),running=host?processor.dawIsPlaying():processor.rhythmIsRunning();
        play.setButtonText(host?"ARM":"PLAY");play.setTooltip(host?"Armar el ritmo: arranca y se detiene con Play/Stop del DAW.":"Reproducir o parar el ritmo.");
        transport.setText(juce::String(host?"DAW":"INTERNO")+(running?"  > PLAY":"  [] STOP")+"  |  "+juce::String(processor.getCurrentTempo(),1)+" BPM"+(host && processor.state.getRawParameterValue("rhythmEnabled")->load()<.5f?"  |  DESARMADO":""),juce::dontSendNotification);
        if(revision!=processor.getSampleRevision() || shownLoop!=looping) {
            revision=processor.getSampleRevision();shownLoop=looping;const auto names=processor.sampleNames();const char* tracks[]{"KICK","SNARE","HAT","AUX","PERC"};
            const auto hits=processor.drumChoices();
            for(size_t t=0;t<5;++t){samples[t].clear(juce::dontSendNotification);samples[t].addItem(juce::String(tracks[t])+": Auto",1);for(const auto& item:hits)samples[t].addItem(item.second,item.first+2);}
            load.setTooltip(names.isEmpty()?"Carga un ZIP con samples para escuchar la bateria.":juce::String(names.size())+" samples cargados. Puedes asignar cada pista.");
            patternAttachment.reset();pattern.onChange=nullptr;pattern.clear(juce::dontSendNotification);
            if(looping) {
                for(const auto& item:processor.sampleChoices(true))pattern.addItem(item.second,item.first+1);
                pattern.onChange=[this]{if(pattern.getSelectedId()>0){auto* p=processor.state.getParameter("rhythmLoop");p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(pattern.getSelectedId()-1)));}};
            } else {
                for(int i=0;i<7;++i)pattern.addItem(chordharp::rhythmNames[static_cast<size_t>(i)],i+1);
                patternAttachment=std::make_unique<juce::ComboBoxParameterAttachment>(*processor.state.getParameter("rhythmPattern"),pattern);
                pattern.onChange=[this]{const int selected=pattern.getSelectedId()-1;if(selected>=0 && selected<6){auto* length=processor.state.getParameter("rhythmLength");length->beginChangeGesture();length->setValueNotifyingHost(length->convertTo0to1(selected==5?12.f:16.f));length->endChangeGesture();}};
            }
        }
        if(looping)pattern.setSelectedId(static_cast<int>(processor.state.getRawParameterValue("rhythmLoop")->load())+1,juce::dontSendNotification);
        loopNotice.setVisible(looping);ruler.setVisible(!looping);ruler.repaint();swing.setEnabled(!looping);
        loopNotice.setText("LOOP GRABADO - "+pattern.getText()+"\nTempo original: 100 BPM | Actual: "+juce::String(processor.getCurrentTempo(),1)+" BPM\nAfinacion conservada. Desactiva LOOP para editar golpes, swing y longitud.",juce::dontSendNotification);
        const int selected=static_cast<int>(processor.state.getRawParameterValue("rhythmPattern")->load());const int length=processor.rhythmPatternLength();
        const auto accent=sampleui::palette(processor,"color-ffb66b44"),ink=sampleui::palette(processor,"color-ff263e38"),button=sampleui::palette(processor,"color-ffdad3bc");
        sampleui::colours(processor,*this);tempo.setEnabled(!processor.tempoComesFromHost());
        for(size_t t=0;t<5;++t) {
            samples[t].setVisible(!looping);
            samples[t].setSelectedId(static_cast<int>(processor.state.getRawParameterValue("rhythmSample"+juce::String(static_cast<int>(t)))->load())+1,juce::dontSendNotification);
            for(size_t s=0;s<16;++s) {
                auto& b=steps[t][s];const bool on=selected<6?(chordharp::rhythmPatterns[static_cast<size_t>(selected)][t]&(1u<<s))!=0:processor.state.getRawParameterValue(ChordHarpProcessor::rhythmStepID(static_cast<int>(t),static_cast<int>(s)))->load()>.5f;
                b.setVisible(!looping && static_cast<int>(s)<length);b.setToggleState(on,juce::dontSendNotification);b.setEnabled(static_cast<int>(s)<length);
                b.setColour(juce::TextButton::buttonColourId,(s/2)%2?button.darker(.18f):button);b.setColour(juce::TextButton::buttonOnColourId,(s/2)%2?accent.darker(.18f):accent);b.setColour(juce::TextButton::textColourOffId,ink);b.setColour(juce::TextButton::textColourOnId,ink.contrasting());
            }
        }
    }
};
