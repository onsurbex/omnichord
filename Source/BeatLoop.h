#pragma once
#include "SampleAudio.h"

namespace chordharp {
// Offline samples allow reading future grains without adding audio latency.
// Two overlapping grains run at the sample's native pitch; their placement
// follows PPQ. A short waveform-similarity search joins their tails (WSOLA).
// The stereo pair always shares the same offsets. No allocation or file I/O.
class BeatLoop {
    const SampleAudio* active=nullptr;
    double rate=48000,step=1,previous=0,current=0,lastBeat=0;
    int hop=480,at=0;bool ready=false;
    std::array<float,2048> fade{};
    double wrap(double position) const {
        const double size=static_cast<double>(active->frames());
        position=std::fmod(position,size);return position<0?position+size:position;
    }
    float mono(double position) const {
        const auto p=wrap(position);return (active->read(0,p)+active->read(1,p))*.5f;
    }
    double match(double expected) const {
        const double tail=previous+hop*step;
        const int stride=std::max(1,(hop+95)/96),radius=static_cast<int>(active->rate*.006);
        std::array<float,128> reference{};int length=0;
        for(int i=0;i<hop;i+=stride)reference[static_cast<size_t>(length++)]=mono(tail+i*step);
        const auto error=[&](double offset) {
            double sum=0;
            for(int i=0;i<length;++i) {
                const double difference=reference[static_cast<size_t>(i)]-mono(expected+offset+i*stride*step);
                sum+=difference*difference;
            }
            // Prefer timing close to the grid when several offsets match.
            return sum+std::abs(offset)*1e-9;
        };
        double best=0,bestError=error(0);
        if(bestError<1e-12)return expected;
        for(int offset=-radius;offset<=radius;offset+=8) {
            const auto e=error(offset);if(e<bestError){best=offset;bestError=e;}
        }
        const int coarse=static_cast<int>(best);
        for(int offset=std::max(-radius,coarse-7);offset<=std::min(radius,coarse+7);++offset) {
            const auto e=error(offset);if(e<bestError){best=offset;bestError=e;}
        }
        return expected+best;
    }
public:
    void prepare(double sampleRate) {
        rate=sampleRate;hop=std::clamp(static_cast<int>(rate*.01),64,2048);
        for(int i=0;i<hop;++i)fade[static_cast<size_t>(i)]=static_cast<float>(.5-.5*std::cos(3.141592653589793*(i+.5)/hop));
        reset();
    }
    void reset(){active=nullptr;ready=false;at=0;}
    std::array<float,2> next(const SampleAudio* audio,double beat,double beatsPerSample,bool seek=false) {
        if(!audio || audio->frames()<4 || audio->beats<=0){reset();return {};}
        const bool discontinuity=ready && std::abs(beat-lastBeat-beatsPerSample)>.01;
        active=audio;step=audio->rate/rate;
        const double expected=beat*static_cast<double>(audio->frames())/audio->beats;
        if(!ready || audio!=lastSound || seek || discontinuity) {
            current=expected;previous=expected-hop*step;at=0;ready=true;
        } else if(at>=hop) {
            previous=current;current=match(expected);at=0;
        }
        lastSound=audio;lastBeat=beat;
        const double a=wrap(previous+(at+hop)*step),b=wrap(current+at*step);
        const float weight=fade[static_cast<size_t>(at++)];std::array<float,2> out{};
        for(size_t c=0;c<2;++c)out[c]=active->read(c,a)*(1-weight)+active->read(c,b)*weight;
        return out;
    }
private:
    const SampleAudio* lastSound=nullptr;
};
}
