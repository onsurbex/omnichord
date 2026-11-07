#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace omnichord {
inline constexpr float maximumGain=1.995262315f; // +6 dB.
inline juce::String gainText(float gain) {
    if(gain<=0.f)return "-inf dB";
    const float db=20.f*std::log10(gain);
    return (db>.05f?juce::String("+"):juce::String{})+juce::String(std::abs(db)<.05f?0.f:db,1)+" dB";
}
inline float gainFromText(const juce::String& text) {
    if(text.containsIgnoreCase("inf"))return 0;
    return std::clamp(std::pow(10.f,text.getFloatValue()/20.f),0.f,maximumGain);
}
inline juce::AudioParameterFloatAttributes gainAttributes() {
    return juce::AudioParameterFloatAttributes().withLabel("dB")
        .withStringFromValueFunction([](float v,int){return gainText(v);})
        .withValueFromStringFunction([](const juce::String& v){return gainFromText(v);});
}
inline juce::AudioParameterFloatAttributes widthAttributes() {
    return juce::AudioParameterFloatAttributes().withLabel("%")
        .withStringFromValueFunction([](float value,int){return juce::String(juce::roundToInt(value*100))+" %";})
        .withValueFromStringFunction([](const juce::String& text){return std::clamp(text.getFloatValue()*.01f,0.f,2.f);});
}
}
