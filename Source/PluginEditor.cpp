#include "PluginEditor.h"
#include "BinaryData.h"
#include "KeyboardMapping.h"
#include "MacKeyboard.h"
#include "Themes.h"

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
    explicit LogoView(ChordHarpProcessor& p):processor(p),logo(embeddedLogo()) {}
    void paint(juce::Graphics& g) override {
        if(processor.state.getRawParameterValue("theme")->load()>0.5f) {
            g.setColour(cream); g.fillRoundedRectangle(getLocalBounds().toFloat(),12);
        }
        g.setColour(juce::Colours::white);
        g.drawImageWithin(logo,0,0,getWidth(),getHeight(),juce::RectanglePlacement::centred);
    }
    void configure(foleys::GuiItem&) {}
    static std::vector<std::pair<juce::String,int>> colours() { return {}; }
    static std::vector<foleys::SettableProperty> properties(const juce::ValueTree&) { return {}; }
private:
    ChordHarpProcessor& processor;
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
    void update() override {
        value=juce::jlimit(0,47,static_cast<int>(getProperty("button-radio-value")));
        button.setButtonText(getProperty("text").toString());
        button.setRadioGroupId(1001); button.setClickingTogglesState(true);
        button.onClick=[this] { processor.selectChord(value); };
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
    builder->registerFactory("OmniToggle",&ToggleItem::factory);
    builder->registerFactory("OmniChord",&ChordItem::factory);
    builder->registerFactory("ThemeSelector",&InstrumentItem<ThemeSelector>::factory);
    return builder;
}
}

ChordHarpEditor::ChordHarpEditor(ChordHarpProcessor& processor)
    :foleys::MagicPluginEditor(processor.magicState,createBuilder(processor)),processor(processor) {
    setWantsKeyboardFocus(true);
    timerCallback(); startTimerHz(20);
}
ChordHarpEditor::~ChordHarpEditor() { stopTimer(); releaseComputerKeys(); }
void ChordHarpEditor::timerCallback() {
    const int theme=static_cast<int>(processor.state.getRawParameterValue("theme")->load());
    auto layout=processor.magicState.getGuiTree();
    if(theme!=currentTheme || layout!=themedLayout) {
        currentTheme=theme; themedLayout=layout;
        omnichord::applyTheme(layout,theme);
        getGUIBuilder().updateColours(); repaint();
    }
    if(!processor.computerKeyboardEnabled() && !heldKeys.empty()) releaseComputerKeys();
}
void ChordHarpEditor::releaseComputerKeys() {
    for(const auto& held:heldKeys) if(held.second>=0) processor.magicState.getKeyboardState().noteOff(1,held.second,0);
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
            if(it->second>=0) processor.magicState.getKeyboardState().noteOff(1,it->second,0);
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
    const int chord=chordharp::chordFromComputerKey(character,key.getModifiers().isCtrlDown(),key.getModifiers().isShiftDown());
    const int harp=physical?native.harp:chordharp::harpFromComputerKey(character);
    if(chord<0 && (harp<0 || key.getModifiers().isCtrlDown())) return false;
    const int token=physical?0x10000+native.physical:key.getKeyCode();
    if(heldKeys.find(token)==heldKeys.end()) {
        if(chord>=0) { processor.selectChord(chord); heldKeys.emplace(token,-1); }
        else { heldKeys.emplace(token,72+harp); processor.magicState.getKeyboardState().noteOn(1,72+harp,0.8f); }
    }
    return true;
}
