#pragma once
#include "Engine.h"
#include <array>
#include <string_view>

namespace chordharp {
// Ableton: C0=24. Four selector octaves end at B3=71; harp starts at C4=72.
inline constexpr std::array<int,12> omnichordRoots{1,8,3,10,5,0,7,2,9,4,11,6};
inline int chordFromMidi(int note) {
    if(note<24 || note>=72) return -1;
    return ((note-24)/12)*12+omnichordRoots[static_cast<size_t>((note-24)%12)];
}
inline int midiFromChord(int chord) {
    chord=std::clamp(chord,0,47);
    for(size_t i=0;i<omnichordRoots.size();++i)
        if(omnichordRoots[i]==chord%12) return 24+(chord/12)*12+static_cast<int>(i);
    return 24;
}
inline int harpFromMidi(int note,int chord,bool quantized) {
    if(!quantized) return note;
    const int position=std::clamp(note-72,0,55);
    return std::clamp(chordNote(chord%12,chord/12,position%12)+12*(position/12),0,127);
}
inline char32_t upperKey(char32_t c) {
    if(c>=U'a' && c<=U'z') return c-U'a'+U'A';
    return c==U'ñ'?U'Ñ':c;
}
inline int chordFromComputerKey(char32_t key,bool control,bool shift) {
    key=upperKey(key);
    constexpr std::array<std::u32string_view,3> rows{U"QWERTYUIOP",U"ASDFGHJKLÑ",U"ZXCVBNM,.-"};
    for(size_t quality=0;quality<rows.size();++quality) {
        const auto column=rows[quality].find(key);
        if(column==std::u32string_view::npos || (control && column>1)) continue;
        const int position=control?static_cast<int>(column):static_cast<int>(column)+2;
        const int q=quality==1 && shift?3:static_cast<int>(quality);
        return q*12+omnichordRoots[static_cast<size_t>(position)];
    }
    return -1;
}
inline int harpFromComputerKey(char32_t key) {
    if(key>=U'1' && key<=U'9') return static_cast<int>(key-U'1');
    if(key==U'0') return 9;
    if(key==U'\'') return 10;
    if(key==U'¡' || key==U'=') return 11;
    return -1;
}
inline int chromaticFromComputerKey(char32_t key,int octave) {
    key=upperKey(key);const int base=12*(std::clamp(octave,-2,6)+2);
    constexpr std::u32string_view piano=U"AWSEDFTGYHUJKOLPÑ";
    const auto found=piano.find(key);if(found!=std::u32string_view::npos)return base+static_cast<int>(found);
    const int upper=harpFromComputerKey(key);return upper<0?-1:base+12+upper;
}
}
