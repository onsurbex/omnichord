#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace chordharp {
// Additive pipe ranks and nine tonewheel footages; no third-party samples.
inline double organRank(int model,double phase,double frequency,double rate,float tone) {
    constexpr double tau=6.283185307179586;
    double wave=0,energy=0;
    const auto partial=[&](double ratio,double amount) {
        if(frequency*ratio>=rate*0.45 || amount==0) return;
        wave+=amount*std::sin(tau*phase*ratio); energy+=amount*amount;
    };
    if(model>=8) {
        constexpr double ratios[]{0.5,1.5,1,2,3,4,5,6,8};
        constexpr int stops[3][9]{{8,8,8,0,0,0,0,0,0},{8,8,8,8,0,0,0,0,8},{8,8,8,8,8,8,8,8,8}};
        for(int h=0;h<9;++h) {
            const int stop=stops[std::clamp(model-8,0,2)][h];
            if(stop==0) continue;
            double ratio=ratios[h];
            while(frequency*ratio>5920 && ratio>=1) ratio*=0.5; // Upper tonewheel foldback.
            const double colour=h>=3?0.25+0.75*tone:1.0;
            partial(ratio,std::pow(2.0,(stop-8)/2.0)*colour);
        }
    } else {
        constexpr double ranks[4][12]{
            {1,.38,.25,.16,.11,.075,.05,.035,.02,.015,.01,.006},
            {1,.8,.52,.6,.3,.38,.18,.35,.12,.15,.07,.18},
            {1,.27,.38,.1,.22,.08,.12,.04,.07,.03,.04,.02},
            {1,.12,.018,.08,.012,.025,.005,.01,.003,.004,.002,.002}
        };
        const int rank=std::clamp(model-4,0,3);
        for(int h=1;h<=12;++h) partial(h,ranks[rank][h-1]*(h==1?1:0.15+0.85*tone));
        if(model==5 || model==6) partial(0.5,model==5?0.28:0.18);
    }
    return energy>0?wave*1.12/std::sqrt(energy):0;
}
}
