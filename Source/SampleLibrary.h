#pragma once
#include <JuceHeader.h>
#include "SampleAudio.h"
#include "GraySlate.h"

struct SampleEntry {
    juce::String name,path;
    chordharp::SampleAudio audio;
    juce::File recording;
    juce::File archive;
    bool wholeChord=false;
    int bits=32;
};
struct SampleLibrary {
    std::vector<SampleEntry> entries;
    juce::String source;
    std::array<int,5> autoDrums{{-1,-1,-1,-1,-1}};
    std::vector<int> instruments,loops;
    static std::unique_ptr<SampleLibrary> readZip(const juce::File&,juce::String& error);
    static juce::File bundledDirectory();
    bool appendGraySlate(const juce::File& directory,juce::String& error);
    static bool decodeRecording(const juce::File&,chordharp::SampleAudio&,juce::String& error,const juce::File& archive={});
    static std::shared_ptr<const chordharp::GraySlateBank> graySlateBank(const juce::File&,bool harp,juce::String& error);
    int drumForTrack(int track) const;
};
