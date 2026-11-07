#pragma once
#include "FM.h"
namespace chordharp {
inline constexpr std::array<const char*,8> fmPatchNames{{"FM Piano","FM Bell","FM Brass","FM Pad","FM Bass","FM Pluck","FM Glass","FM Strings"}};
inline FMPatch synthPatch(int index,bool organ) {
    index=std::clamp(index,0,7);FMPatch p;
    constexpr int algorithms[]{4,4,0,15,17,6,11,18};p.algorithm=algorithms[index];
    constexpr float ratios[8][6]{{1,1,1,14,1,1},{1,2.7f,2,3.14f,3,1.414f},{1,1,1,2,1,1},{1,1,2,1,3,1},{.5f,1,1,2,1,1},{1,2,1,3,1,2},{1,2.414f,2,3.732f,.5f,5},{1,2,1,1,2,1}};
    for(int i=0;i<6;++i) {
        auto& o=p.op[static_cast<size_t>(i)];const bool carrier=fmCarrier(p.algorithm,i);
        o.ratio=ratios[index][i];o.level=carrier?.9f:.55f;o.velocity=carrier?.35f:.8f;
        o.attack=index==3?.22f:index==2?.025f:index==7?.04f:.002f;
        o.decay=index==1?2.5f:index==4?.25f:index==3?1.8f:.8f;
        o.sustain=organ?(carrier?.8f:.45f):(index==3?.3f:0.f);
        o.release=index==1?2.2f:index==4?.15f:index==3?1.8f:.7f;
        if(!carrier) {o.decay*=.45f;o.release*=.6f;}
    }
    if(index==0) {p.op[3].level=.12f;p.op[1].level=.65f;p.op[5].level=.35f;}
    if(index==4 || index==7)p.feedback=3;
    if(index==1 || index==6)p.feedback=1;
    return p;
}
}
