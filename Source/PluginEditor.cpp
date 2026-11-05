#include "PluginEditor.h"
#include "BinaryData.h"
#include "KeyboardMapping.h"

namespace {
const juce::Colour ink(0xff263e38), cream(0xffeee6cf), accent(0xffb66b44);
const char* noteNames[]{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
juce::Image embeddedLogo() {
    return juce::ImageCache::getFromMemory(BinaryData::BlinblinLogo_png,BinaryData::BlinblinLogo_pngSize);
}
}

void InstrumentLook::drawButtonBackground(juce::Graphics& g,juce::Button& button,
                                         const juce::Colour& colour,bool hover,bool down) {
    auto area=button.getLocalBounds().toFloat().reduced(1.5f);
    if(button.getRadioGroupId()==1001) {
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
void InstrumentLook::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                                     float position,float start,float end,juce::Slider& slider) {
    auto area=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),
                                   static_cast<float>(width),static_cast<float>(height)).reduced(13);
    const float radius=juce::jmax(1.0f,juce::jmin(area.getWidth(),area.getHeight())*0.5f);
    const auto centre=area.getCentre();
    const auto body=slider.findColour(juce::Slider::thumbColourId);
    const auto indicator=slider.findColour(juce::Slider::rotarySliderFillColourId);
    for(int i=0;i<11;++i) {
        const float angle=start+(end-start)*static_cast<float>(i)/10;
        g.setColour(body.withAlpha(i<=position*10?0.8f:0.25f));
        g.drawLine({centre.getPointOnCircumference(radius+4,angle),centre.getPointOnCircumference(radius+7,angle)},1.2f);
    }
    const auto knob=juce::Rectangle<float>(radius*2,radius*2).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.15f)); g.fillEllipse(knob.translated(0,3));
    g.setGradientFill(juce::ColourGradient(body.brighter(0.15f),knob.getTopLeft(),body.darker(0.2f),knob.getBottomRight(),false));
    g.fillEllipse(knob); g.setColour(cream.withAlpha(0.35f)); g.drawEllipse(knob.reduced(2),1);
    const float angle=start+position*(end-start);
    g.setColour(indicator);
    g.drawLine({centre.getPointOnCircumference(radius*0.56f,angle),centre.getPointOnCircumference(radius*0.86f,angle)},2.5f);
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
    explicit LogoView(ChordHarpProcessor&):logo(embeddedLogo()) {}
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colours::white);
        g.drawImageWithin(logo,0,0,getWidth(),getHeight(),juce::RectanglePlacement::centred);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    juce::Image logo;
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
        const int chord=juce::jlimit(0,35,static_cast<int>(processor.state.getRawParameterValue("chord")->load()));
        g.setColour(findColour(text).withAlpha(0.6f)); g.setFont(juce::FontOptions(10.0f));
        g.drawText(caption,20,9,getWidth()-40,20,juce::Justification::centredLeft);
        g.setColour(findColour(text)); g.setFont(juce::FontOptions(fontSize));
        g.drawText(juce::String(noteNames[chord%12])+(chord/12==1?"m":chord/12==2?"7":""),20,30,getWidth()-40,getHeight()-34,juce::Justification::centredLeft);
        g.setColour(accent); g.fillEllipse(static_cast<float>(getWidth()-35),static_cast<float>(getHeight()/2-4),8,8);
    }
private:
    void timerCallback() override {
        const int current=static_cast<int>(processor.state.getRawParameterValue("chord")->load());
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

std::unique_ptr<foleys::MagicGUIBuilder> createBuilder(ChordHarpProcessor& processor) {
    auto builder=std::make_unique<foleys::MagicGUIBuilder>(processor.magicState);
    builder->registerJUCEFactories();
    builder->registerJUCELookAndFeels();
    auto look=std::make_unique<InstrumentLook>();
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
    return builder;
}
}

ChordHarpEditor::ChordHarpEditor(ChordHarpProcessor& processor)
    :foleys::MagicPluginEditor(processor.magicState,createBuilder(processor)),processor(processor) {
    setWantsKeyboardFocus(true);
}
ChordHarpEditor::~ChordHarpEditor() { releaseComputerKeys(); }
void ChordHarpEditor::releaseComputerKeys() {
    for(const auto& held:heldKeys) processor.magicState.getKeyboardState().noteOff(1,held.second,0);
    heldKeys.clear();
}
void ChordHarpEditor::focusLost(FocusChangeType) { releaseComputerKeys(); }
bool ChordHarpEditor::keyStateChanged(bool) {
    bool handled=false;
    for(auto it=heldKeys.begin();it!=heldKeys.end();) {
        if(!juce::KeyPress::isKeyCurrentlyDown(it->first)) {
            processor.magicState.getKeyboardState().noteOff(1,it->second,0);
            it=heldKeys.erase(it); handled=true;
        } else ++it;
    }
    return handled;
}
bool ChordHarpEditor::keyPressed(const juce::KeyPress& key) {
    if(!processor.computerKeyboardEnabled() || getGUIBuilder().isEditModeOn()) return false;
    if(key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown()) return false;
    const auto* focus=juce::Component::getCurrentlyFocusedComponent();
    if(focus && (dynamic_cast<const juce::TextEditor*>(focus) || focus->findParentComponentOfClass<juce::TextEditor>()
        || dynamic_cast<const juce::ComboBox*>(focus) || focus->findParentComponentOfClass<juce::ComboBox>())) return false;
    const int code=key.getKeyCode();
    if(code<0 || code>127) return false;
    const int upper=juce::CharacterFunctions::toUpperCase(static_cast<juce::juce_wchar>(code));
    const juce::String qualities("ZXC");
    const int quality=qualities.indexOfChar(static_cast<juce::juce_wchar>(upper));
    if(quality>=0) { processor.selectChord(quality*12+processor.selectedChord()%12); return true; }
    const int column=juce::String("1234567890QP").indexOfChar(static_cast<juce::juce_wchar>(upper));
    const int harp=juce::String("AWSEDFTGYHUJ").indexOfChar(static_cast<juce::juce_wchar>(upper));
    int note=-1;
    if(column>=0) note=36+(processor.selectedChord()/12)*12+column;
    else if(harp>=0) note=72+harp;
    if(note<0) return false;
    if(heldKeys.find(upper)==heldKeys.end()) {
        heldKeys.emplace(upper,note);
        processor.magicState.getKeyboardState().noteOn(1,note,0.8f);
    }
    return true;
}
