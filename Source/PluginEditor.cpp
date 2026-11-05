#include "PluginEditor.h"
#include "BrandAssets.h"

namespace {
const juce::Colour ink(0xff263e38), cream(0xffeee6cf), accent(0xffb66b44);
constexpr int fifths[]{5,0,7,2,9,4,11,6,1,8,3,10};
const char* noteNames[]{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
juce::Image embeddedLogo() {
    return juce::ImageCache::getFromMemory(ChordHarpBrand::BlinblinLogo_png,
                                          ChordHarpBrand::BlinblinLogo_pngSize);
}
}

void InstrumentLook::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                         const juce::Colour& colour, bool hover, bool down) {
    auto area=button.getLocalBounds().toFloat().reduced(1.5f);
    if(button.getProperties().getWithDefault("chordKey",false)) {
        const auto fill=button.getToggleState()?accent:colour;
        g.setColour(ink.withAlpha(0.25f)); g.fillEllipse(area.translated(0,2));
        g.setGradientFill(juce::ColourGradient(fill.brighter(down?0.0f:0.2f),area.getX(),area.getY(),
                                             fill.darker(down?0.15f:0.08f),area.getX(),area.getBottom(),false));
        g.fillEllipse(area);
        g.setColour(ink.withAlpha(hover?0.65f:0.25f)); g.drawEllipse(area,1);
    } else {
        g.setColour(hover?colour.brighter(0.08f):colour); g.fillRoundedRectangle(area,7);
        g.setColour(ink.withAlpha(0.22f)); g.drawRoundedRectangle(area,7,1);
    }
}
void InstrumentLook::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                                     float position,float start,float end,juce::Slider&) {
    auto area=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),
                                   static_cast<float>(width),static_cast<float>(height)).reduced(13);
    const float radius=juce::jmin(area.getWidth(),area.getHeight())*0.5f;
    const auto centre=area.getCentre();
    for(int i=0;i<11;++i) {
        const float angle=start+(end-start)*static_cast<float>(i)/10;
        g.setColour(ink.withAlpha(i<=position*10?0.8f:0.25f));
        g.drawLine({centre.getPointOnCircumference(radius+4,angle),centre.getPointOnCircumference(radius+7,angle)},1.2f);
    }
    const auto knob=juce::Rectangle<float>(radius*2,radius*2).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.15f)); g.fillEllipse(knob.translated(0,3));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff496057),knob.getTopLeft(),juce::Colour(0xff1c302a),knob.getBottomRight(),false));
    g.fillEllipse(knob); g.setColour(cream.withAlpha(0.35f)); g.drawEllipse(knob.reduced(2),1);
    const float angle=start+position*(end-start);
    g.setColour(cream);
    g.drawLine({centre.getPointOnCircumference(radius*0.56f,angle),centre.getPointOnCircumference(radius*0.86f,angle)},2.5f);
}
void StrumStrip::paint(juce::Graphics& g) {
    auto area=getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff324b40),0,0,juce::Colour(0xff132b23),area.getWidth(),area.getHeight(),false));
    g.fillRoundedRectangle(area,22);
    for(int i=0;i<12;++i) {
        const float y=(11.5f-static_cast<float>(i))*getHeight()/12.0f;
        const bool active=i==previous;
        g.setColour(active?juce::Colour(0xffffd993):juce::Colour(0xffb9ae7a));
        g.drawLine(24,y+5,static_cast<float>(getWidth()-24),y-5,active?3.5f:1.4f);
        g.setColour(cream.withAlpha(0.5f)); g.fillEllipse(13,y-1,2,2);
    }
    g.setColour(cream.withAlpha(0.15f)); g.drawRoundedRectangle(area.reduced(1),22,1);
}
void StrumStrip::play(int y) {
    const int current=11-juce::jlimit(0,11,y*12/juce::jmax(1,getHeight()));
    if(previous==current) return;
    if(previous<0) processor.strum(current);
    else { const int step=current>previous?1:-1; for(int i=previous+step;i!=current+step;i+=step) processor.strum(i); }
    previous=current; repaint();
}
ChordHarpEditor::ChordHarpEditor(ChordHarpProcessor& p):AudioProcessorEditor(p),processor(p),strip(p) {
    look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffdcd4bc));
    look.setColour(juce::TextButton::buttonOnColourId,accent);
    look.setColour(juce::TextButton::textColourOffId,ink);
    look.setColour(juce::TextButton::textColourOnId,juce::Colours::white);
    look.setColour(juce::ToggleButton::textColourId,ink);
    look.setColour(juce::ToggleButton::tickColourId,accent);
    setLookAndFeel(&look);
    logo=embeddedLogo();
    logoButton.setButtonText(logo.isValid()?"CAMBIAR LOGO":"CARGAR LOGO");
    for(int i=0;i<36;++i) {
        auto& button=chords[static_cast<size_t>(i)];
        button.setButtonText(juce::String(noteNames[i%12])+(i/12==1?"m":i/12==2?"7":""));
        button.getProperties().set("chordKey",true);
        button.setTooltip("Seleccionar "+button.getButtonText()+" y rasguear en el panel derecho");
        button.onClick=[this,i] {
            auto* parameter=processor.state.getParameter("chord");
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(i))); parameter->endChangeGesture();
            timerCallback(); repaint();
        };
        addAndMakeVisible(button);
    }
    const char* ids[]{"gain","tone","sustain","vibrato"};
    const char* titles[]{"VOLUMEN","TIMBRE","SUSTAIN","VIBRATO"};
    for(size_t i=0;i<4;++i) {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,65,19);
        knobs[i].setColour(juce::Slider::textBoxTextColourId,ink);
        knobs[i].setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        addAndMakeVisible(knobs[i]);
        labels[i].setText(titles[i],juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred); labels[i].setColour(juce::Label::textColourId,ink);
        addAndMakeVisible(labels[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,ids[i],knobs[i]);
    }
    addAndMakeVisible(strip); addAndMakeVisible(pad); addAndMakeVisible(stop); addAndMakeVisible(logoButton);
    strip.setTooltip("Mantén pulsado y desliza arriba o abajo para tocar las notas del acorde");
    padAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"pad",pad);
    stop.onClick=[this] {
        auto* parameter=processor.state.getParameter("pad");
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(0); parameter->endChangeGesture(); processor.panic.store(true);
    };
    logoButton.onClick=[this] { chooseLogo(); };
    logoButton.setTooltip("Carga tu PNG original. Se guarda dentro del estado del plugin en Ableton.");
    setSize(1060,650); startTimerHz(15); timerCallback();
}
ChordHarpEditor::~ChordHarpEditor() { stopTimer(); setLookAndFeel(nullptr); }
void ChordHarpEditor::chooseLogo() {
    logoChooser=std::make_unique<juce::FileChooser>("Selecciona tu logotipo (PNG o JPEG)",juce::File{},"*.png;*.jpg;*.jpeg");
    const juce::Component::SafePointer<ChordHarpEditor> safe(this);
    logoChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& chooser) {
            if(!safe) return;
            const auto file=chooser.getResult(); if(!file.existsAsFile()) return;
            juce::MemoryBlock bytes;
            if(file.getSize()>8*1024*1024 || !file.loadFileAsData(bytes)) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Logo","Usa un PNG o JPEG de menos de 8 MB."); return;
            }
            const auto image=juce::ImageFileFormat::loadFrom(bytes.getData(),bytes.getSize());
            if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Logo","Usa una imagen PNG o JPEG de hasta 4096 x 4096 píxeles."); return;
            }
            safe->processor.state.state.setProperty("customLogo",bytes.toBase64Encoding(),nullptr);
            safe->processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
            safe->timerCallback();
        });
}
void ChordHarpEditor::timerCallback() {
    const int selected=static_cast<int>(processor.state.getRawParameterValue("chord")->load());
    bool changed=false;
    for(int i=0;i<36;++i) {
        auto& button=chords[static_cast<size_t>(i)];
        if(button.getToggleState()!=(i==selected)) changed=true;
        button.setToggleState(i==selected,juce::dontSendNotification);
    }
    const auto encoded=processor.state.state.getProperty("customLogo").toString();
    if(encoded!=logoData) {
        logoData=encoded; juce::MemoryBlock bytes;
        logo=bytes.fromBase64Encoding(encoded)?juce::ImageFileFormat::loadFrom(bytes.getData(),bytes.getSize()):juce::Image{};
        if(!logo.isValid()) logo=embeddedLogo();
        logoButton.setButtonText(logo.isValid()?"CAMBIAR LOGO":"CARGAR LOGO");
        changed=true;
    }
    if(changed) repaint();
}
void ChordHarpEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff172a25));
    auto shell=getLocalBounds().toFloat().reduced(15);
    g.setColour(juce::Colours::black.withAlpha(0.3f)); g.fillRoundedRectangle(shell.translated(0,5),55);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff5efdf),30,25,juce::Colour(0xffddd0ad),1000,630,false));
    g.fillRoundedRectangle(shell,55); g.setColour(juce::Colours::white.withAlpha(0.6f)); g.drawRoundedRectangle(shell.reduced(2),53,1);
    auto text=[&g](const juce::String& value,float size,int x,int y,int w,int h,bool bold=false) {
        g.setFont(juce::FontOptions(size,bold?juce::Font::bold:juce::Font::plain));
        g.drawText(value,x,y,w,h,juce::Justification::centredLeft);
    };
    g.setColour(ink); text("CHORDHARP  /  INSTRUMENTO ELECTRONICO",13,75,53,520,22);
    text("Acordes al tacto.",40,74,76,520,55,true);
    text("36 ACORDES   /   12 CUERDAS   /   UN GESTO",12,77,133,500,20);
    g.setColour(ink.withAlpha(0.07f)); g.fillRoundedRectangle(77,175,150,80,15);
    for(int row=0;row<7;++row) for(int column=0;column<16;++column) {
        g.setColour(ink.withAlpha(0.4f)); g.fillEllipse(88.0f+column*8,185.0f+row*9,2.5f,2.5f);
    }
    const int selected=static_cast<int>(processor.state.getRawParameterValue("chord")->load());
    g.setColour(ink); g.fillRoundedRectangle(260,175,290,80,13);
    g.setColour(cream.withAlpha(0.6f)); text("ACORDE EN MEMORIA",10,280,187,175,16);
    g.setColour(cream); text(juce::String(noteNames[selected%12])+(selected/12==1?"m":selected/12==2?"7":""),28,280,205,190,40);
    g.setColour(accent); g.fillEllipse(515,206,8,8);
    g.setColour(ink.withAlpha(0.14f)); g.drawLine(77,275,558,275,1); g.drawLine(77,439,558,439,1);
    g.setColour(ink); text("MAY",10,43,299,35,20); text("MIN",10,43,343,35,20); text("7",10,43,387,35,20);
    g.setColour(ink.withAlpha(0.16f)); g.fillRoundedRectangle(620,161,370,413,33);
    g.setColour(cream); g.fillRoundedRectangle(623,163,364,407,31);
    g.setColour(ink); text("OMNI HARP",12,650,177,300,22,true); text("DESLIZA ARRIBA O ABAJO",11,650,536,300,20);
    if(logo.isValid()) {
        g.setColour(juce::Colours::white); g.drawImageWithin(logo,660,49,280,86,juce::RectanglePlacement::centred);
    } else {
        g.setColour(ink); text("BLINBLIN",36,700,55,280,50,true);
    }
    g.setColour(ink.withAlpha(0.5f)); text("BLINBLIN  /  CHORDHARP 01",10,720,597,260,20);
}
void ChordHarpEditor::resized() {
    for(int i=0;i<4;++i) {
        knobs[static_cast<size_t>(i)].setBounds(75+i*120,450,100,103);
        labels[static_cast<size_t>(i)].setBounds(75+i*120,552,100,20);
    }
    pad.setBounds(78,588,230,28); stop.setBounds(405,586,140,30);
    for(int row=0;row<3;++row) for(int column=0;column<12;++column)
        chords[static_cast<size_t>(row*12+fifths[column])].setBounds(82+column*39+row*5,291+row*44,35,35);
    strip.setBounds(646,208,318,320); logoButton.setBounds(746,132,112,23);
}
