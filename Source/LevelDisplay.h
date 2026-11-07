#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace omnichord {
inline juce::String gainText(float gain) {
    return gain<=0.f?juce::String("-inf dB"):juce::String(20.f*std::log10(gain),1)+" dB";
}
inline float gainFromText(const juce::String& text) {
    if(text.containsIgnoreCase("inf"))return 0;
    return std::clamp(std::pow(10.f,text.getFloatValue()/20.f),0.f,1.f);
}
inline juce::AudioParameterFloatAttributes gainAttributes() {
    return juce::AudioParameterFloatAttributes().withLabel("dB")
        .withStringFromValueFunction([](float v,int){return gainText(v);})
        .withValueFromStringFunction([](const juce::String& v){return gainFromText(v);});
}
}
