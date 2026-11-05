#pragma once
#include <JuceHeader.h>
#include <array>

namespace omnichord {
// The first theme resolves every palette entry to the colour in the user's XML.
struct Theme { const char* name; juce::uint32 panel,ink,accent,button,outer,harp,string; };
inline const std::array<Theme,6> themes{{
    {"Personalizado",0xffeee6cf,0xff263e38,0xffb66b44,0xffdad3bc,0xff172a25,0xff193328,0xffb9ae7a},
    {"Crema",0xfff2e7cf,0xff3f4335,0xffbd704a,0xffded2b6,0xff303a30,0xff30433b,0xffd4b77a},
    {"Medianoche",0xff242c40,0xffe3e8f4,0xff96a9ef,0xff39445e,0xff121827,0xff141d30,0xff9fadd5},
    {"Bosque",0xffdce7d8,0xff253e32,0xff55836b,0xffbdcebb,0xff1a3028,0xff243f34,0xffc2d6a0},
    {"Coral",0xfff7dfd4,0xff613d42,0xffc56c69,0xffe6beb5,0xff54343b,0xff5b3940,0xffedbd98},
    {"Lavanda",0xffe9e1f2,0xff453d60,0xff9674b7,0xffcec0de,0xff332b48,0xff403455,0xffcbb0e6}
}};
inline juce::StringArray themeNames() { juce::StringArray names; for(const auto& t:themes) names.add(t.name); return names; }
inline bool isColourProperty(const juce::Identifier& id) {
    const auto s=id.toString();
    return s.contains("color") || s.contains("colour") || (s.startsWith("slider-") && !s.contains("type") && !s.contains("textbox"))
        || s=="rotary-fill" || s=="rotary-outline" || s=="label-text" || (s.startsWith("button-") && !s.contains("radio"))
        || s.startsWith("toggle-") || s.startsWith("combo-") || s=="display-text" || s=="display-background" || s=="harp-background";
}
inline void paletteReferences(juce::ValueTree node,juce::ValueTree palette) {
    if(node.hasType("Palettes") || node.hasType("CustomColours")) return;
    for(int i=0;i<node.getNumProperties();++i) {
        const auto key=node.getPropertyName(i); const auto value=node.getProperty(key).toString();
        if(isColourProperty(key) && value.length()==8 && value.containsOnly("0123456789abcdefABCDEF") && !value.startsWith("00")) {
            const juce::Identifier name("color-"+value.toLowerCase());
            if(!palette.hasProperty(name)) palette.setProperty(name,value,nullptr);
            node.setProperty(key,"$"+name.toString(),nullptr);
        }
    }
    for(auto child:node) paletteReferences(child,palette);
}
inline void prepareThemes(juce::ValueTree layout) {
    auto style=layout.getChildWithName("Styles").getChild(0);
    if(!style.isValid()) return;
    auto palettes=style.getOrCreateChildWithName("Palettes",nullptr);
    if(!palettes.getNumChildren()) palettes.appendChild(juce::ValueTree("default"),nullptr);
    paletteReferences(layout,palettes.getChild(0));
    auto custom=layout.getOrCreateChildWithName("CustomColours",nullptr);
    auto palette=palettes.getChild(0);
    for(int i=0;i<palette.getNumProperties();++i) {
        const auto name=palette.getPropertyName(i);
        if(name.toString().startsWith("color-") && !custom.hasProperty(name)) custom.setProperty(name,palette.getProperty(name),nullptr);
    }
}
inline juce::Colour themedColour(juce::Colour original,int index) {
    index=juce::jlimit(0,static_cast<int>(themes.size())-1,index);
    if(index==0) return original;
    const auto& t=themes[static_cast<size_t>(index)]; const auto rgb=original.getARGB()&0x00ffffff;
    juce::uint32 colour=t.ink;
    switch(rgb) {
        case 0xeee6cf: case 0xf6efdc: colour=t.panel; break;
        case 0x263e38: colour=t.ink; break;
        case 0xb66b44: colour=t.accent; break;
        case 0xdad3bc: case 0xddd3b8: colour=t.button; break;
        case 0x172a25: colour=t.outer; break;
        case 0x193328: colour=t.harp; break;
        case 0xb9ae7a: case 0xffd993: colour=t.string; break;
        default: colour=original.getBrightness()>0.65f?t.panel:t.ink; break;
    }
    return juce::Colour(colour).withAlpha(original.getFloatAlpha());
}
inline void captureCustomTheme(juce::ValueTree layout) {
    auto palette=layout.getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
    auto custom=layout.getOrCreateChildWithName("CustomColours",nullptr);
    for(int i=0;i<palette.getNumProperties();++i) {
        const auto name=palette.getPropertyName(i);
        if(name.toString().startsWith("color-")) custom.setProperty(name,palette.getProperty(name),nullptr);
    }
}
inline void applyTheme(juce::ValueTree layout,int index) {
    auto palette=layout.getChildWithName("Styles").getChild(0).getChildWithName("Palettes").getChild(0);
    const auto custom=layout.getChildWithName("CustomColours");
    for(int i=0;i<palette.getNumProperties();++i) {
        const auto name=palette.getPropertyName(i);
        if(name.toString().startsWith("color-")) {
            const auto original=juce::Colour::fromString(name.toString().substring(6));
            const auto colour=index==0?custom.getProperty(name,original.toString()).toString():themedColour(original,index).toString();
            palette.setProperty(name,colour,nullptr);
        }
    }
}
}
