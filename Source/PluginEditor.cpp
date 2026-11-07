#include "PluginEditor.h"
#include "BinaryData.h"
#include "KeyboardMapping.h"
#include "MacKeyboard.h"
#include "Themes.h"
#include "PresetBar.h"
#include "DelayTimeControl.h"
#include "FMEditor.h"
#include "SampleRhythmEditor.h"
#include "MixerPanel.h"
#include "ModeLabels.h"

namespace {
const juce::Colour ink(0xff263e38), cream(0xffeee6cf), accent(0xffb66b44);
const char* noteNames[]{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
juce::String tabParameter(const juce::String& name) {
    if(name=="Harp")return "harpEnabled";if(name=="Organo" || name=="Organ")return "organEnabled";
    if(name=="FM")return "fmEnabled";if(name=="Samples")return "samplesEnabled";if(name=="Ritmos")return "rhythmEnabled";
    if(name=="FX")return "fxEnabled";if(name=="Delay")return "delayEnabled";if(name=="Reverb")return "reverbEnabled";if(name=="Tape")return "tapeEnabled";
    return {};
}
class TabLed final : public juce::TextButton,private juce::Timer {
    ChordHarpProcessor& processor;std::unique_ptr<juce::ButtonParameterAttachment> attachment;
    std::array<std::atomic<float>*,2> solos{};bool hadSolo=false;
    bool soloActive() const {return (solos[0] && solos[0]->load()>.5f) || (solos[1] && solos[1]->load()>.5f);}
    void timerCallback() override {
        const bool solo=soloActive();if(solo || solo!=hadSolo)repaint();
        if(solo!=hadSolo)setTooltip(solo?"Solo activo. Desactivalo en el mixer; apagar el canal tambien lo quita.":"Activar/desactivar sin cambiar de pestana");
        hadSolo=solo;
    }
public:
    TabLed(ChordHarpProcessor& p,const juce::String& id):processor(p) {
        setComponentID("tab-led-"+id);setClickingTogglesState(true);setWantsKeyboardFocus(false);setMouseCursor(juce::MouseCursor::PointingHandCursor);setTooltip("Activar/desactivar sin cambiar de pestana");
        attachment=std::make_unique<juce::ButtonParameterAttachment>(*p.state.getParameter(id),*this);
        for(int i=0;i<7;++i)if(id==p.mixerParameter(i,"Enabled"))solos[0]=p.state.getRawParameterValue(p.mixerParameter(i,"Solo"));
        if(id=="fxEnabled"){solos[0]=p.state.getRawParameterValue("delaySolo");solos[1]=p.state.getRawParameterValue("reverbSolo");}
        timerCallback();startTimerHz(20);
    }
    void paintButton(juce::Graphics& g,bool hover,bool) override {
        const bool solo=soloActive(),lit=getToggleState() && (!solo || (juce::Time::getMillisecondCounter()/250)%2==0);
        const auto colour=solo?juce::Colour(0xfff0ca59):sampleui::palette(processor,"color-ffb66b44"),ink=sampleui::palette(processor,"color-ff263e38");
        auto r=juce::Rectangle<float>(7,7).withCentre(getLocalBounds().toFloat().getCentre());
        g.setColour(ink.withAlpha(.45f));g.fillEllipse(r.expanded(1));
        g.setColour(lit?colour.brighter(hover?.3f:.1f):ink.withAlpha(.18f));g.fillEllipse(r);
        g.setColour(juce::Colours::white.withAlpha(lit?.65f:.25f));g.fillEllipse(r.getX()+1.3f,r.getY()+1,2,1.5f);
    }
};
void installTabLeds(juce::Component& parent,ChordHarpProcessor& p,std::vector<std::unique_ptr<juce::Component>>& owned) {
    if(auto* button=dynamic_cast<juce::TabBarButton*>(&parent)) {
        const auto id=tabParameter(button->getButtonText());
        if(id.isNotEmpty()) {
            juce::Component* led=nullptr;for(int i=0;i<button->getNumChildComponents();++i)if(button->getChildComponent(i)->getComponentID()=="tab-led-"+id)led=button->getChildComponent(i);
            if(!led){auto created=std::make_unique<TabLed>(p,id);led=created.get();button->addAndMakeVisible(*created);owned.push_back(std::move(created));}
            led->setBounds(button->getWidth()-17,2,15,juce::jmax(8,button->getHeight()-4));
        }
    }
    for(int i=0;i<parent.getNumChildComponents();++i)installTabLeds(*parent.getChildComponent(i),p,owned);
}
juce::Image embeddedLogo() {
    return juce::ImageCache::getFromMemory(BinaryData::BlinblinLogo_png,BinaryData::BlinblinLogo_pngSize);
}
void preserveBackgroundPainting(juce::Component& component) {
    // Foleys marks a viewport opaque even when its decorator has rounded corners.
    // Let JUCE paint the editor behind these views instead of culling its fill.
    if(dynamic_cast<juce::Viewport*>(&component) && dynamic_cast<foleys::GuiItem*>(component.getParentComponent()))
        component.setOpaque(false);
    for(int i=0;i<component.getNumChildComponents();++i) preserveBackgroundPainting(*component.getChildComponent(i));
}
}

int InstrumentLook::getTabButtonBestWidth(juce::TabBarButton& b,int) {
    return juce::jmax(38,b.getTabbedButtonBar().getWidth()/juce::jmax(1,b.getTabbedButtonBar().getNumTabs())-1);
}
void InstrumentLook::drawTabButton(juce::TabBarButton& b,juce::Graphics& g,bool hover,bool) {
    if(!processor){LookAndFeel_V4::drawTabButton(b,g,hover,false);return;}
    const auto panel=sampleui::palette(*processor,"color-ffeee6cf"),ink=sampleui::palette(*processor,"color-ff263e38"),accent=sampleui::palette(*processor,"color-ffb66b44");
    const auto r=b.getLocalBounds().toFloat().reduced(.5f);g.setColour(b.isFrontTab()?panel.brighter(.035f):panel.darker(hover?.03f:.08f));g.fillRoundedRectangle(r,4);
    g.setColour(ink.withAlpha(.25f));g.drawRoundedRectangle(r,4,.7f);if(b.isFrontTab()){g.setColour(accent);g.fillRect(2,0,b.getWidth()-4,2);}
    g.setColour(ink);g.setFont(juce::FontOptions(11.f,b.isFrontTab()?juce::Font::bold:juce::Font::plain));
    auto text=b.getLocalBounds().reduced(4,1);if(tabParameter(b.getButtonText()).isNotEmpty())text.removeFromRight(13);
    g.drawFittedText(b.getButtonText(),text,juce::Justification::centred,1);
}
void InstrumentLook::drawButtonBackground(juce::Graphics& g,juce::Button& button,
                                         const juce::Colour& colour,bool hover,bool down) {
    auto area=button.getLocalBounds().toFloat().reduced(1.5f);
    if(static_cast<bool>(button.getProperties().getWithDefault("physical-switch",false))) {
        const bool on=button.getToggleState();
        auto cap=area.withTrimmedBottom(3).translated(0,on || down?2.0f:0.0f);
        const auto fill=on?button.findColour(juce::TextButton::buttonOnColourId):colour;
        g.setColour(juce::Colours::black.withAlpha(0.4f)); g.fillRoundedRectangle(area.translated(0,2),5);
        g.setGradientFill(juce::ColourGradient(fill.brighter(hover?0.22f:0.12f),cap.getTopLeft(),fill.darker(0.2f),cap.getBottomLeft(),false));
        g.fillRoundedRectangle(cap,5);
        g.setColour(juce::Colours::white.withAlpha(0.35f)); g.drawRoundedRectangle(cap.reduced(1),4,0.7f);
        g.setColour(on?juce::Colour(0xfffff1b0):juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(cap.getX()+8,cap.getCentreY()-2.5f,5,5);
    } else if(button.getRadioGroupId()==1001) {
        const auto fill=button.getToggleState()?button.findColour(juce::TextButton::buttonOnColourId):colour;
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
void InstrumentLook::drawButtonText(juce::Graphics& g,juce::TextButton& button,bool hover,bool down) {
    if(!static_cast<bool>(button.getProperties().getWithDefault("physical-switch",false))) {
        LookAndFeel_V4::drawButtonText(g,button,hover,down); return;
    }
    g.setColour(button.findColour(button.getToggleState()?juce::TextButton::textColourOnId:juce::TextButton::textColourOffId));
    g.setFont(juce::FontOptions(static_cast<bool>(button.getProperties().getWithDefault("compact-switch",false))?10.f:12.f,juce::Font::bold));
    auto text=button.getLocalBounds().reduced(5,2).withTrimmedLeft(13).translated(0,button.getToggleState() || down?1:0);
    g.drawFittedText(button.getButtonText(),text,juce::Justification::centred,1);
}
void InstrumentLook::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float position,float minimum,float maximum,juce::Slider::SliderStyle style,juce::Slider& slider) {
    if(!static_cast<bool>(slider.getProperties()["centre-fill"]) || style!=juce::Slider::LinearHorizontal) {
        LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,position,minimum,maximum,style,slider);return;
    }
    const float centre=x+width*.5f,cy=y+height*.5f;
    g.setColour(slider.findColour(juce::Slider::backgroundColourId));g.fillRoundedRectangle(static_cast<float>(x),cy-2,static_cast<float>(width),4,2);
    g.setColour(slider.findColour(juce::Slider::trackColourId));g.fillRoundedRectangle(std::min(centre,position),cy-2,std::max(.1f,std::abs(position-centre)),4,2);
    g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(.45f));g.drawLine(centre,cy-4,centre,cy+4,1);
    g.setColour(slider.findColour(juce::Slider::thumbColourId));g.fillEllipse(position-3.5f,cy-3.5f,7,7);
}
void InstrumentLook::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                                     float position,float start,float end,juce::Slider& slider) {
    auto area=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),
                                   static_cast<float>(width),static_cast<float>(height)).reduced(height<60?6.0f:13.0f);
    const float radius=juce::jmax(1.0f,juce::jmin(area.getWidth(),area.getHeight())*0.5f);
    const auto centre=area.getCentre();
    const auto body=slider.findColour(juce::Slider::thumbColourId);
    const auto indicator=slider.findColour(juce::Slider::rotarySliderFillColourId);
    const bool spread=static_cast<bool>(slider.getProperties()["width-spread"]);
    for(int i=0;i<11;++i) {
        const float angle=start+(end-start)*static_cast<float>(i)/10;
        const bool active=spread?std::abs(i-5)<=position*5+.01f:i<=position*10;
        g.setColour((active?indicator:body).withAlpha(active?.9f:.22f));
        if(spread){const auto dot=centre.getPointOnCircumference(radius+5,angle);g.fillEllipse(dot.x-1.2f,dot.y-1.2f,2.4f,2.4f);}
        else g.drawLine({centre.getPointOnCircumference(radius+4,angle),centre.getPointOnCircumference(radius+7,angle)},1.2f);
    }
    const auto knob=juce::Rectangle<float>(radius*2,radius*2).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.15f)); g.fillEllipse(knob.translated(0,3));
    g.setGradientFill(juce::ColourGradient(body.brighter(0.15f),knob.getTopLeft(),body.darker(0.2f),knob.getBottomRight(),false));
    g.fillEllipse(knob); g.setColour(cream.withAlpha(0.35f)); g.drawEllipse(knob.reduced(2),1);
    g.setColour(indicator);
    const auto needle=[&](float angle){g.drawLine({centre.getPointOnCircumference(radius*.46f,angle),centre.getPointOnCircumference(radius*.86f,angle)},2.f);};
    if(spread){const float mid=(start+end)*.5f,open=position*(end-start)*.5f;needle(mid-open);needle(mid+open);}
    else needle(start+position*(end-start));
}

StrumStrip::StrumStrip(ChordHarpProcessor& p):processor(p) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setColour(backgroundColour,juce::Colour(0xff193328));
    setColour(stringColour,juce::Colour(0xffb9ae7a));
    setColour(activeColour,juce::Colour(0xffffd993));
}
std::vector<std::pair<juce::String,int>> StrumStrip::colours() {
    return {{"harp-background",backgroundColour},{"string-color",stringColour},{"active-color",activeColour}};
}
std::vector<foleys::SettableProperty> StrumStrip::properties(const juce::ValueTree& node) {
    return {{node,"string-width",foleys::SettableProperty::Number,1.4f,{}}};
}
void StrumStrip::configure(foleys::GuiItem& item) {
    const auto value=item.getProperty("string-width");
    stringWidth=juce::jlimit(0.5f,6.0f,value.isVoid()?1.4f:static_cast<float>(value));
}
void StrumStrip::paint(juce::Graphics& g) {
    auto area=getLocalBounds().toFloat();
    const auto background=findColour(backgroundColour);
    g.setGradientFill(juce::ColourGradient(background.brighter(0.1f),0,0,background.darker(0.2f),area.getWidth(),area.getHeight(),false));
    g.fillRoundedRectangle(area,22);
    for(int i=0;i<12;++i) {
        const float y=(11.5f-static_cast<float>(i))*getHeight()/12.0f;
        g.setColour(findColour(i==previous?activeColour:stringColour));
        g.drawLine(24,y+5,static_cast<float>(getWidth()-24),y-5,i==previous?stringWidth+2:stringWidth);
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

namespace {
class LogoView final : public juce::Component {
public:
    explicit LogoView(ChordHarpProcessor& p):processor(p),logo(embeddedLogo()),
        whiteLogo(juce::ImageCache::getFromMemory(BinaryData::BlinblinLogo_white_png,BinaryData::BlinblinLogo_white_pngSize)) {}
    void paint(juce::Graphics& g) override {
        const auto theme=static_cast<int>(processor.state.getRawParameterValue("theme")->load());
        const auto background=omnichord::logoBackground(processor.magicState.getGuiTree(),theme);
        const auto& image=omnichord::useWhiteLogo(background)?whiteLogo:logo;
        g.setColour(juce::Colours::white);
        g.drawImageWithin(image,0,0,getWidth(),getHeight(),juce::RectanglePlacement::centred);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    ChordHarpProcessor& processor;
    juce::Image logo,whiteLogo;
};

class ChordDisplay final : public juce::Component,private juce::Timer {
public:
    enum Colours { background=0x2010010, text };
    explicit ChordDisplay(ChordHarpProcessor& p):processor(p) {
        setColour(background,ink); setColour(text,cream); startTimerHz(15);
    }
    void configure(foleys::GuiItem& item) {
        caption=item.getProperty("display-caption").toString();
        const auto size=item.getProperty("display-font-size");
        fontSize=juce::jlimit(10.0f,100.0f,size.isVoid()?28.0f:static_cast<float>(size));
    }
    static std::vector<std::pair<juce::String,int>> colours() {
        return {{"display-background",background},{"display-text",text}};
    }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree& node) {
        return {{node,"display-caption",foleys::SettableProperty::Text,"ACORDE EN MEMORIA",{}},
                {node,"display-font-size",foleys::SettableProperty::Number,28.0f,{}}};
    }
    void paint(juce::Graphics& g) override {
        g.setColour(findColour(background)); g.fillRoundedRectangle(getLocalBounds().toFloat(),13);
        const int chord=processor.selectedChord();
        g.setColour(findColour(text).withAlpha(0.6f)); g.setFont(juce::FontOptions(10.0f));
        g.drawText(caption,20,9,getWidth()-40,20,juce::Justification::centredLeft);
        g.setColour(findColour(text)); g.setFont(juce::FontOptions(fontSize));
        g.drawText(juce::String(noteNames[chord%12])+(chord/12==1?"m":chord/12==2?"7":chord/12==3?"Maj7":""),20,30,getWidth()-40,getHeight()-34,juce::Justification::centredLeft);
        g.setColour(accent); g.fillEllipse(static_cast<float>(getWidth()-35),static_cast<float>(getHeight()/2-4),8,8);
    }
private:
    void timerCallback() override {
        const int current=processor.selectedChord();
        if(current!=selected) { selected=current; repaint(); }
    }
    ChordHarpProcessor& processor;
    juce::String caption;
    float fontSize=28;
    int selected=-1;
};

class Speaker final : public juce::Component {
public:
    enum Colours { dotColour=0x2010020 };
    explicit Speaker(ChordHarpProcessor&) { setColour(dotColour,ink.withAlpha(0.4f)); }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {{"dot-color",dotColour}}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
    void paint(juce::Graphics& g) override {
        g.setColour(findColour(dotColour));
        for(int row=0;row<7;++row) for(int col=0;col<16;++col)
            g.fillEllipse((col+1)*getWidth()/17.0f,(row+1)*getHeight()/8.0f,2.5f,2.5f);
    }
};

ChordHarpProcessor& processorFor(foleys::MagicGUIBuilder& builder) {
    auto* processor=dynamic_cast<ChordHarpProcessor*>(builder.getMagicState().getProcessor());
    jassert(processor!=nullptr);
    return *processor;
}

template<class Widget> class InstrumentItem final : public foleys::GuiItem {
public:
    InstrumentItem(foleys::MagicGUIBuilder& builder,const juce::ValueTree& node)
        :foleys::GuiItem(builder,node),widget(processorFor(builder)) {
        setColourTranslation(Widget::colours()); addAndMakeVisible(widget);
    }
    static std::unique_ptr<foleys::GuiItem> factory(foleys::MagicGUIBuilder& builder,const juce::ValueTree& node) {
        return std::make_unique<InstrumentItem>(builder,node);
    }
    void update() override { widget.configure(*this); }
    juce::Component* getWrappedComponent() override { return &widget; }
    std::vector<foleys::SettableProperty> getSettableProperties() const override { return Widget::properties(configNode); }
private:
    Widget widget;
};

// One parameter attachment per boolean button. Foleys' radio handler must not
// write the same parameter a second time with an empty radio value.
class ToggleItem final : public foleys::GuiItem {
public:
    ToggleItem(foleys::MagicGUIBuilder& b,const juce::ValueTree& n):GuiItem(b,n) {
        setColourTranslation({{"toggle-text",juce::ToggleButton::textColourId},{"toggle-tick",juce::ToggleButton::tickColourId},
                              {"toggle-tick-disabled",juce::ToggleButton::tickDisabledColourId}});
        addAndMakeVisible(button);
    }
    static std::unique_ptr<foleys::GuiItem> factory(foleys::MagicGUIBuilder& b,const juce::ValueTree& n) { return std::make_unique<ToggleItem>(b,n); }
    void update() override {
        attachment.reset(); button.setButtonText(getProperty("text").toString());
        const auto id=getProperty("parameter").toString();
        if(id.isNotEmpty()) attachment=getMagicState().createAttachment(id,button);
    }
    juce::Component* getWrappedComponent() override { return &button; }
    std::vector<foleys::SettableProperty> getSettableProperties() const override {
        return {{configNode,"text",foleys::SettableProperty::Text,{},{}},
                {configNode,"parameter",foleys::SettableProperty::Choice,{},magicBuilder.createParameterMenuLambda()}};
    }
private:
    juce::ToggleButton button;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

class SwitchItem final : public foleys::GuiItem {
public:
    SwitchItem(foleys::MagicGUIBuilder& b,const juce::ValueTree& n):GuiItem(b,n) {
        setColourTranslation({{"button-color",juce::TextButton::buttonColourId},{"button-on-color",juce::TextButton::buttonOnColourId},
                              {"button-off-text",juce::TextButton::textColourOffId},{"button-on-text",juce::TextButton::textColourOnId}});
        button.getProperties().set("physical-switch",true);
        button.setClickingTogglesState(true); addAndMakeVisible(button);
    }
    static std::unique_ptr<foleys::GuiItem> factory(foleys::MagicGUIBuilder& b,const juce::ValueTree& n) { return std::make_unique<SwitchItem>(b,n); }
    void update() override {
        attachment.reset(); button.setButtonText(getProperty("text").toString());
        const auto id=getProperty("parameter").toString();
        if(id.isNotEmpty()) attachment=getMagicState().createAttachment(id,button);
    }
    juce::Component* getWrappedComponent() override { return &button; }
    std::vector<foleys::SettableProperty> getSettableProperties() const override {
        return {{configNode,"text",foleys::SettableProperty::Text,{},{}},
                {configNode,"parameter",foleys::SettableProperty::Choice,{},magicBuilder.createParameterMenuLambda()}};
    }
private:
    juce::TextButton button;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

// Virtual chord values 36..47 use the separate Maj7 parameter, preserving the
// original 0..35 chord automation range for existing Ableton projects.
class ChordItem final : public foleys::GuiItem,private juce::Timer {
public:
    ChordItem(foleys::MagicGUIBuilder& b,const juce::ValueTree& n):GuiItem(b,n),processor(processorFor(b)) {
        setColourTranslation({{"button-color",juce::TextButton::buttonColourId},{"button-on-color",juce::TextButton::buttonOnColourId},
                              {"button-off-text",juce::TextButton::textColourOffId},{"button-on-text",juce::TextButton::textColourOnId}});
        addAndMakeVisible(button); startTimerHz(30);
    }
    static std::unique_ptr<foleys::GuiItem> factory(foleys::MagicGUIBuilder& b,const juce::ValueTree& n) { return std::make_unique<ChordItem>(b,n); }
    ~ChordItem() override { if(pressed) processor.releaseChord(value); }
    void update() override {
        if(pressed) { processor.releaseChord(value); pressed=false; }
        value=juce::jlimit(0,47,static_cast<int>(getProperty("button-radio-value")));
        button.setButtonText(getProperty("text").toString());
        button.setRadioGroupId(1001); button.setClickingTogglesState(true);
        button.onStateChange=[this] {
            const bool down=button.isDown();
            if(down==pressed) return;
            pressed=down;
            if(down) processor.pressChord(value); else processor.releaseChord(value);
        };
        timerCallback();
    }
    juce::Component* getWrappedComponent() override { return &button; }
    std::vector<foleys::SettableProperty> getSettableProperties() const override {
        return {{configNode,"text",foleys::SettableProperty::Text,{},{}},
                {configNode,"button-radio-value",foleys::SettableProperty::Number,0,{}}};
    }
private:
    void timerCallback() override { button.setToggleState(processor.selectedChord()==value,juce::dontSendNotification); }
    ChordHarpProcessor& processor;
    juce::TextButton button;
    int value=0;
    bool pressed=false;
};

class ThemeLook final : public juce::LookAndFeel_V4 {
    static void swatches(juce::Graphics& g,int theme,int x,int y) {
        const auto& t=omnichord::themes[static_cast<size_t>(juce::jlimit(0,5,theme))];
        const juce::uint32 colours[]{t.panel,t.ink,t.accent,t.harp};
        for(int i=0;i<4;++i) {
            auto box=juce::Rectangle<float>(static_cast<float>(x+i*14),static_cast<float>(y),11,11);
            g.setColour(juce::Colour(colours[i])); g.fillRoundedRectangle(box,2);
            g.setColour(juce::Colours::grey.withAlpha(0.5f)); g.drawRoundedRectangle(box,2,0.5f);
        }
    }
public:
    void drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,bool separator,bool active,bool highlighted,
                          bool ticked,bool submenu,const juce::String& text,const juce::String& shortcut,
                          const juce::Drawable* icon,const juce::Colour* colour) override {
        LookAndFeel_V4::drawPopupMenuItem(g,area,separator,active,highlighted,ticked,submenu,text,shortcut,icon,colour);
        const int index=omnichord::themeNames().indexOf(text);
        if(index>=0) swatches(g,index,area.getRight()-68,area.getCentreY()-5);
    }
    void getIdealPopupMenuItemSize(const juce::String& text,bool separator,int height,int& width,int& outHeight) override {
        LookAndFeel_V4::getIdealPopupMenuItemSize(text,separator,height,width,outHeight); width+=74; outHeight=juce::jmax(28,outHeight);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool down,int bx,int by,int bw,int bh,juce::ComboBox& combo) override {
        LookAndFeel_V4::drawComboBox(g,w,h,down,bx,by,bw,bh,combo);
        swatches(g,combo.getSelectedId()-1,8,h/2-5);
    }
    void positionComboBoxText(juce::ComboBox& combo,juce::Label& label) override {
        LookAndFeel_V4::positionComboBoxText(combo,label);
        label.setBounds(68,1,juce::jmax(1,combo.getWidth()-96),combo.getHeight()-2);
    }
};
class ThemeSelector final : public juce::ComboBox {
public:
    explicit ThemeSelector(ChordHarpProcessor& p) {
        setLookAndFeel(&look); addItemList(omnichord::themeNames(),1);
        attachment=std::make_unique<juce::ComboBoxParameterAttachment>(*p.state.getParameter("theme"),*this);
    }
    ~ThemeSelector() override { setLookAndFeel(nullptr); }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() {
        return {{"combo-background",juce::ComboBox::backgroundColourId},{"combo-text",juce::ComboBox::textColourId},
                {"combo-outline",juce::ComboBox::outlineColourId},{"combo-text",juce::ComboBox::arrowColourId}};
    }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    ThemeLook look;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
};

class InstrumentBody final : public juce::Component {
public:
    explicit InstrumentBody(ChordHarpProcessor& p):processor(p) { setInterceptsMouseClicks(false,false); }
    void paint(juce::Graphics& g) override {
        const auto panel=omnichord::logoBackground(processor.magicState.getGuiTree(),static_cast<int>(processor.state.getRawParameterValue("theme")->load()));
        const auto area=getLocalBounds().toFloat().reduced(1);
        g.setColour(panel.darker(0.5f)); g.fillRoundedRectangle(area,54);
        auto face=area.withTrimmedBottom(9);
        g.setGradientFill(juce::ColourGradient(panel.brighter(0.06f),face.getTopLeft(),panel.darker(0.08f),face.getBottomLeft(),false));
        g.fillRoundedRectangle(face,53);
        for(int i=0;i<4;++i) {
            g.setColour(juce::Colours::white.withAlpha(0.17f-static_cast<float>(i)*0.03f));
            g.drawRoundedRectangle(face.reduced(2.0f+i),51.0f-i,1.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.18f)); g.drawRoundedRectangle(area.reduced(1),53,1);
        // Fine moulded texture stays behind the functional controls.
        g.setColour(juce::Colours::black.withAlpha(0.018f));
        for(int y=25;y<getHeight()-25;y+=12) for(int x=45;x<getWidth()-45;x+=12) g.fillRect(x+(y%3),y,1,1);
        for(const auto point:{juce::Point<float>(32,32),juce::Point<float>(area.getRight()-32,32),
                              juce::Point<float>(32,area.getBottom()-36),juce::Point<float>(area.getRight()-32,area.getBottom()-36)}) {
            const juce::Rectangle<float> screw(point.x-8,point.y-8,16,16);
            g.setColour(juce::Colours::black.withAlpha(.32f));g.fillEllipse(screw.expanded(1).translated(0,1));
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe1e4e8),screw.getTopLeft(),juce::Colour(0xff777e89),screw.getBottomRight(),false));
            g.fillEllipse(screw);
            g.setColour(juce::Colour(0xff545a64));g.drawEllipse(screw,.8f);
            juce::Path star;star.addStar(point,6,2.2f,4.6f,.15f);
            g.setColour(juce::Colours::white.withAlpha(.45f));g.fillPath(star,juce::AffineTransform::translation(0,.65f));
            g.setColour(juce::Colour(0xff343942));g.fillPath(star);
        }
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    ChordHarpProcessor& processor;
};
class InstrumentShadow final : public juce::Component {
public:
    explicit InstrumentShadow(ChordHarpProcessor&) { setInterceptsMouseClicks(false,false); }
    void paint(juce::Graphics& g) override {
        juce::Path path; path.addRoundedRectangle(getLocalBounds().toFloat().reduced(12),54);
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f),12,{0,5}).drawForPath(g,path);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
};

std::unique_ptr<foleys::MagicGUIBuilder> createBuilder(ChordHarpProcessor& processor) {
    auto builder=std::make_unique<foleys::MagicGUIBuilder>(processor.magicState);
    builder->registerJUCEFactories();
    builder->registerJUCELookAndFeels();
    auto look=std::make_unique<InstrumentLook>(&processor);
    look->setColour(juce::TextButton::buttonColourId,juce::Colour(0xffdad3bc));
    look->setColour(juce::TextButton::buttonOnColourId,accent);
    look->setColour(juce::Slider::thumbColourId,ink);
    look->setColour(juce::Slider::rotarySliderFillColourId,accent);
    look->setColour(juce::ToggleButton::textColourId,ink);
    look->setColour(juce::ToggleButton::tickColourId,accent);
    builder->registerLookAndFeel("BLINBLIN",std::move(look));
    builder->registerFactory("OmniStrum",&InstrumentItem<StrumStrip>::factory);
    builder->registerFactory("BlinblinLogo",&InstrumentItem<LogoView>::factory);
    builder->registerFactory("ChordDisplay",&InstrumentItem<ChordDisplay>::factory);
    builder->registerFactory("Speaker",&InstrumentItem<Speaker>::factory);
    builder->registerFactory("OmniToggle",&ToggleItem::factory);
    builder->registerFactory("OmniSwitch",&SwitchItem::factory);
    builder->registerFactory("OmniChord",&ChordItem::factory);
    builder->registerFactory("InstrumentBody",&InstrumentItem<InstrumentBody>::factory);
    builder->registerFactory("InstrumentShadow",&InstrumentItem<InstrumentShadow>::factory);
    builder->registerFactory("DelayTimeControl",&InstrumentItem<DelayTimeControl>::factory);
    builder->registerFactory("PresetBar",&InstrumentItem<PresetBar>::factory);
    builder->registerFactory("SynthSelector",&InstrumentItem<SynthSelector>::factory);
    builder->registerFactory("FMPanel",&InstrumentItem<FMPanel>::factory);
    builder->registerFactory("SamplePanel",&InstrumentItem<SamplePanel>::factory);
    builder->registerFactory("RhythmPanel",&InstrumentItem<RhythmPanel>::factory);
    builder->registerFactory("MixerPanel",&InstrumentItem<MixerPanel>::factory);
    builder->registerFactory("ModeLabels",&InstrumentItem<ModeLabels>::factory);
    builder->registerFactory("ThemeSelector",&InstrumentItem<ThemeSelector>::factory);
    return builder;
}
}

ChordHarpEditor::ChordHarpEditor(ChordHarpProcessor& processor)
    :foleys::MagicPluginEditor(processor.magicState,createBuilder(processor)),processor(processor) {
    setOpaque(true);
    setWantsKeyboardFocus(true);
    timerCallback(); startTimerHz(20);
}
void ChordHarpEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff292a2d)); }
ChordHarpEditor::~ChordHarpEditor() { stopTimer(); releaseComputerKeys(); }
void ChordHarpEditor::timerCallback() {
    const int theme=static_cast<int>(processor.state.getRawParameterValue("theme")->load());
    auto layout=processor.magicState.getGuiTree();
    if(currentTheme==0 && layout==themedLayout) omnichord::captureCustomTheme(layout);
    if(theme!=currentTheme || layout!=themedLayout) {
        currentTheme=theme; themedLayout=layout;
        omnichord::applyTheme(layout,theme);
        getGUIBuilder().updateColours();
        // Foleys RootItem updates its children but skips its own decorator.
        // Set that layer explicitly so it cannot cover the editor with darkgrey.
        if(auto* root=getGUIBuilder().findGuiItemWithId("root")) root->foleys::GuiItem::updateColours();
        repaint();
    }
    preserveBackgroundPainting(*this);
    if((!processor.computerKeyboardEnabled() || processor.chromaticMode()!=lastMode) && !heldKeys.empty())releaseComputerKeys();
    lastMode=processor.chromaticMode();
    std::erase_if(tabLeds,[](const auto& led){return led->getParentComponent()==nullptr;});
    installTabLeds(*this,processor,tabLeds);
}
void ChordHarpEditor::releaseComputerKeys() {
    for(const auto& held:heldKeys) {
        if(held.second>=0) processor.computerNoteOff(held.second);
        else if(held.second<=-2)processor.releaseChord(-held.second-2,true);
    }
    heldKeys.clear();
}
void ChordHarpEditor::focusLost(FocusChangeType) { releaseComputerKeys(); }
bool ChordHarpEditor::keyStateChanged(bool) {
    const auto native=chordharp::currentMacKeyboardEvent();
    bool handled=false;
    for(auto it=heldKeys.begin();it!=heldKeys.end();) {
        const bool released=it->first>=0x10000?(native.isUp && native.physical==it->first-0x10000)
            :!juce::KeyPress::isKeyCurrentlyDown(it->first);
        if(released) {
            if(it->second>=0) processor.computerNoteOff(it->second);
            else if(it->second<=-2)processor.releaseChord(-it->second-2,true);
            it=heldKeys.erase(it); handled=true;
        } else ++it;
    }
    return handled;
}
bool ChordHarpEditor::keyPressed(const juce::KeyPress& key) {
    if(!processor.computerKeyboardEnabled() || getGUIBuilder().isEditModeOn()) return false;
    if(key.getModifiers().isCommandDown() || key.getModifiers().isAltDown()) return false;
    const auto* focus=juce::Component::getCurrentlyFocusedComponent();
    if(focus && (dynamic_cast<const juce::TextEditor*>(focus) || focus->findParentComponentOfClass<juce::TextEditor>()
        || dynamic_cast<const juce::ComboBox*>(focus) || focus->findParentComponentOfClass<juce::ComboBox>())) return false;
    const auto native=chordharp::currentMacKeyboardEvent();
    const bool physical=native.physical>=0 && !native.isUp;
    const auto character=physical?native.character:static_cast<char32_t>(key.getKeyCode());
    const int token=physical?0x10000+native.physical:key.getKeyCode();
    if(processor.chromaticMode()) {
        const auto upper=chordharp::upperKey(character);
        if(upper==U'Z' || upper==U'X') {
            if(heldKeys.find(token)==heldKeys.end()){releaseComputerKeys();processor.shiftComputerOctave(upper==U'Z'?-1:1);heldKeys.emplace(token,-1);}return true;
        }
        const int note=physical && native.harp>=0?12*(processor.computerOctave()+2)+12+native.harp:chordharp::chromaticFromComputerKey(character,processor.computerOctave());
        if(note<0)return false;
        if(heldKeys.find(token)==heldKeys.end()){heldKeys.emplace(token,note);processor.computerNoteOn(note);}
        return true;
    }
    const int chord=chordharp::chordFromComputerKey(character,key.getModifiers().isCtrlDown(),key.getModifiers().isShiftDown());
    const int harp=physical?native.harp:chordharp::harpFromComputerKey(character);
    if(chord<0 && harp<0) return false;
    if(heldKeys.find(token)==heldKeys.end()) {
        if(chord>=0) { processor.pressChord(chord,true); heldKeys.emplace(token,-chord-2); }
        else { heldKeys.emplace(token,72+harp); processor.computerNoteOn(72+harp); }
    }
    return true;
}
