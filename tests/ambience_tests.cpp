#include "AmbienceChecks.h"
#include "../Source/Delay.h"
#include "../Source/Tape.h"
#include <cstdlib>

static void require(bool ok,const char* message) {
    if(!ok) { std::cerr << message << '\n';std::exit(1); }
}
int main() {
    checkAmbienceDSP(require);
    for(double rate:{44100.0,48000.0,96000.0}) {
        chordharp::StereoDelay delay;delay.mix=1;delay.prepare(rate);
        chordharp::Tape tape;tape.enabled=true;tape.wowAmount=1;tape.flutterAmount=1;tape.prepare(rate);
        for(int i=0;i<static_cast<int>(rate*2);++i) {
            if(i%512==0) delay.milliseconds=i%1024==0?6.0f:12000.0f;
            float l=.2f*static_cast<float>(std::sin(i*.17)),r=-l;
            delay.process(l,r);tape.process(l,r);
            require(std::isfinite(l) && std::isfinite(r),"Fractional delay and tape must remain finite");
        }
    }
    std::cout << "Sanitized checks passed: predelay, reflections, three EQ bands, fractional delay and tape\n";
}
