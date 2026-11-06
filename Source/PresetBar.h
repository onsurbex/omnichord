#pragma once
#include "PluginProcessor.h"

class PresetBar final : public juce::Component,private juce::Timer {
public:
    explicit PresetBar(ChordHarpProcessor& p):processor(p) {
        look.setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff35373d));
        look.setColour(juce::ComboBox::textColourId,juce::Colour(0xffeeeeef));
        look.setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff55585f));
        setLookAndFeel(&look);
        addAndMakeVisible(list); addAndMakeVisible(load); addAndMakeVisible(save); addAndMakeVisible(startup); addAndMakeVisible(status);
        for(auto* button:{&load,&save,&startup}) {
            button->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff41444b));
            button->setColour(juce::TextButton::textColourOffId,juce::Colour(0xffeeeeef));
        }
        status.setColour(juce::Label::textColourId,juce::Colour(0xffc3c7d0)); status.setFont(juce::FontOptions(11.0f));
        list.setComponentID("preset-list");load.setComponentID("preset-load");save.setComponentID("preset-save");startup.setComponentID("preset-default");
        list.setTooltip("Presets de fabrica y tu biblioteca personal");
        list.onChange=[this] {
            juce::String error; const int id=list.getSelectedId(); bool ok=false;
            if(id>0 && id<=ChordHarpProcessor::factoryPresetNames().size()) ok=processor.loadFactoryPreset(id-1,error);
            else if(id>=1000 && id<1000+files.size()) ok=processor.loadPreset(files[id-1000],error);
            else return;
            refresh(); message(ok?"Preset cargado: "+processor.getPresetName():error);
        };
        load.onClick=[this] { choose(false); }; save.onClick=[this] { choose(true); };
        startup.onClick=[this] {
            juce::PopupMenu menu; menu.addItem(1,"Usar el estado actual al iniciar"); menu.addItem(2,"Volver al inicio de fabrica");
            juce::Component::SafePointer<PresetBar> safe(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&startup),[safe](int result) {
                if(!safe || result==0) return; juce::String error;
                const bool ok=result==1?safe->processor.setStartupPreset(error):safe->processor.clearStartupPreset(error);
                safe->message(ok?(result==1?"Inicio guardado: "+safe->processor.getPresetName():"Inicio de fabrica seleccionado"):error);
            });
        };
        refresh(); message("Preset de inicio elegible desde Inicio. Los proyectos del DAW conservan su estado."); startTimerHz(2);
    }
    ~PresetBar() override { stopTimer(); setLookAndFeel(nullptr); }
    void resized() override {
        const int width=getWidth(); const int buttonWidth=110;
        list.setBounds(0,0,juce::jmax(120,width-3*buttonWidth-24),27);
        int x=list.getRight()+8;
        for(auto* button:{&load,&save,&startup}) { button->setBounds(x,0,buttonWidth,27); x+=buttonWidth+4; }
        status.setBounds(0,29,width,15);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    void message(const juce::String& text) { status.setText(text,juce::dontSendNotification); }
    void timerCallback() override { if(revision!=processor.getPresetRevision()) refresh(); }
    void refresh() {
        revision=processor.getPresetRevision(); list.clear(juce::dontSendNotification);
        const auto names=ChordHarpProcessor::factoryPresetNames(); list.addItemList(names,1); list.addSeparator();
        files=processor.getPresetDirectory().findChildFiles(juce::File::findFiles,false,"*.omnipreset");
        std::sort(files.begin(),files.end(),[](const auto& a,const auto& b) { return a.getFileName().compareNatural(b.getFileName())<0; });
        for(int i=0;i<files.size();++i) list.addItem(files[i].getFileNameWithoutExtension(),1000+i);
        const auto current=processor.getPresetName(),source=processor.getPresetSource(); int selected=0;
        if(source.startsWith("factory:")) selected=source.substring(8).getIntValue()+1;
        for(int i=0;i<files.size();++i) if("file:"+files[i].getFullPathName()==source) selected=1000+i;
        if(selected<=0) { selected=5000000; list.addItem(current,selected); }
        list.setSelectedId(selected,juce::dontSendNotification);
    }
    void choose(bool saving) {
        const auto directory=processor.getPresetDirectory(); directory.createDirectory();
        const auto initial=saving?directory.getChildFile(juce::File::createLegalFileName(processor.getPresetName())+".omnipreset"):directory;
        chooser=std::make_unique<juce::FileChooser>(saving?"Guardar preset completo":"Cargar preset",initial,"*.omnipreset");
        const int flags=juce::FileBrowserComponent::canSelectFiles | (saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode);
        juce::Component::SafePointer<PresetBar> safe(this);
        chooser->launchAsync(flags,[safe,saving](const juce::FileChooser& dialog) {
            if(!safe || dialog.getResult()==juce::File{}) return;
            juce::String error; const bool ok=saving?safe->processor.savePreset(dialog.getResult(),error):safe->processor.loadPreset(dialog.getResult(),error);
            safe->refresh(); safe->message(ok?(saving?"Preset completo guardado: ":"Preset cargado: ")+safe->processor.getPresetName():error);
        });
    }
    ChordHarpProcessor& processor;
    juce::LookAndFeel_V4 look;
    juce::ComboBox list;
    juce::TextButton load{"Cargar..."},save{"Guardar..."},startup{"Inicio..."};
    juce::Label status;
    juce::Array<juce::File> files;
    std::unique_ptr<juce::FileChooser> chooser;
    unsigned revision=0;
};
