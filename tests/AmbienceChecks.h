#pragma once
#include "../Source/Ambience.h"
#include <iostream>

template<class Check>
void checkAmbienceDSP(Check require) {
    const auto impulse=[&](float predelay,float early) {
        chordharp::Ambience room;room.mix=1;room.predelayMs=predelay;room.early=early;room.prepare(48000);room.configure();
        int first=-1;double energy=0;
        for(int i=0;i<24000;++i) {float l=i==0?1.f:0.f,r=l;room.process(l,r);if(std::abs(l)+std::abs(r)>1e-6 && first<0)first=i;energy+=l*l+r*r;require(std::isfinite(l) && std::isfinite(r),"Reverb output must remain finite");}
        return std::pair<int,double>{first,energy};
    };
    const auto dry=impulse(0,0),pre=impulse(100,0),reflections=impulse(0,1);
    if(pre.first-dry.first<4798 || pre.first-dry.first>4802)
        std::cerr << "Reverb first wet samples: " << dry.first << " without predelay, " << pre.first << " with 100 ms\n";
    require(pre.first-dry.first>=4798 && pre.first-dry.first<=4802,"Predelay must postpone the wet signal by 100 ms");
    require(reflections.first<dry.first && reflections.second>dry.second,"Early reflections must add a separate early response");
    const auto response=[](int band,float gain,double frequency) {
        chordharp::Ambience room;room.mix=1;room.early=1;room.gainDb[static_cast<size_t>(band)]=gain;room.prepare(48000);room.configure();
        // Measure the driven frequency, rather than total RMS that includes
        // the room's decaying startup response at other frequencies.
        double sinL=0,cosL=0,sinR=0,cosR=0;
        for(int i=0;i<192000;++i) {
            const double phase=6.283185307179586*frequency*i/48000,sine=std::sin(phase),cosine=std::cos(phase);
            float l=.1f*static_cast<float>(sine),r=l;room.process(l,r);
            if(i>=96000) { sinL+=l*sine;cosL+=l*cosine;sinR+=r*sine;cosR+=r*cosine; }
        }
        return sinL*sinL+cosL*cosL+sinR*sinR+cosR*cosR;
    };
    constexpr double frequencies[]{100,1000,8000};
    for(int band=0;band<3;++band) {
        const auto boost=response(band,12,frequencies[band]),cut=response(band,-12,frequencies[band]);
        if(!(boost>cut*5)) std::cerr << "Reverb EQ band " << band << " boost=" << boost << " cut=" << cut << '\n';
        require(boost>cut*5,"Each EQ band must change its measured spectral response");
    }
}
