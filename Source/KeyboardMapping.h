#pragma once
#include "Engine.h"
#include <array>

namespace chordharp {
// Ableton Live: MIDI 60 is C3; selector octaves begin at MIDI 36 (C1).
inline constexpr std::array<int,12> omnichordRoots{1,8,3,10,5,0,7,2,9,4,11,6};
inline int chordFromMidi(int note) {
    if(note<36 || note>=72) return -1;
    return ((note-36)/12)*12+omnichordRoots[static_cast<size_t>((note-36)%12)];
}
inline int harpFromMidi(int note,int chord,bool quantized) {
    if(!quantized) return note;
    const int position=std::clamp(note-72,0,55);
    return std::clamp(chordNote(chord%12,chord/12,position%12)+12*(position/12),0,127);
}
}
