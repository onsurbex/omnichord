#pragma once
#include <JuceHeader.h>
#include "SampleAudio.h"

struct SampleEntry {
    juce::String name,path;
    chordharp::SampleAudio audio;
};
struct SampleLibrary {
    std::vector<SampleEntry> entries;
    juce::String source;
    std::array<int,5> autoDrums{{-1,-1,-1,-1,-1}};
    std::vector<int> instruments,loops;
    static std::unique_ptr<SampleLibrary> readZip(const juce::File&,juce::String& error);
    int drumForTrack(int track) const;
};
