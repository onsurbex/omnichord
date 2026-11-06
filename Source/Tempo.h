#pragma once
#include <algorithm>
#include <array>
namespace chordharp {
inline constexpr std::array<double,17> delayBeats{{4,2,4.0/3,3,1,2.0/3,1.5,.5,1.0/3,.75,.25,1.0/6,.375,.125,1.0/12,.1875,.0625}};
inline double syncedDelayMs(double bpm,int division) {
    return 60000.0/std::clamp(bpm,20.0,400.0)*delayBeats[static_cast<size_t>(std::clamp(division,0,16))];
}
}
