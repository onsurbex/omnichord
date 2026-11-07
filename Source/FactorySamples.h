#pragma once
#include <array>

namespace chordharp {
// Roots measured from the supplied Omnichord Vol. 1 recordings. MIDI note
// numbers are independent of the octave names used by individual DAWs.
struct FactorySample { const char* name; int root; float gain; bool sustain; };
inline constexpr std::array<FactorySample,5> factorySamples{{
    {"Omni Axe C",72,2.0f,false}, {"Omni Bass F",41,1.0f,false},
    {"Omni Church C",60,1.5f,true}, {"Omni Piano C",60,1.5f,false},
    {"Omni Voice C",72,1.0f,false}
}};
inline constexpr std::array<const char*,4> factoryLoops{{"4toFloor 100","Dusty hip hop 100","March 100","Tango 100"}};
}
