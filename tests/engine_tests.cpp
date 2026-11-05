#include "Engine.h"
#include "KeyboardMapping.h"
#include "Delay.h"
#include <cstdlib>
#include <iostream>
void require(bool condition,const char* message) { if(!condition) { std::cerr<<message<<'\n'; std::exit(1); } }
int main() {
    using namespace chordharp;
    require(chordNote(0,0,0)==48 && chordNote(0,0,1)==52 && chordNote(0,0,3)==60,"Major voicing");
    require(chordNote(0,1,1)==51 && chordNote(0,2,3)==58,"Minor/seventh voicing");
    for(int quality=0;quality<3;++quality) for(int col=0;col<12;++col)
        require(chordFromMidi(36+quality*12+col)==quality*12+omnichordRoots[static_cast<size_t>(col)],"Omnichord position-to-MIDI mapping");
    require(chordFromMidi(35)==-1 && chordFromMidi(72)==-1,"Selector MIDI boundaries");
    require(chordFromMidi(36)==1 && chordFromMidi(47)==6,"Major row Db to F#");
    require(chordFromMidi(48)==13 && chordFromMidi(71)==30,"Minor/seventh rows");
    require(harpFromMidi(72,1,true)==49 && harpFromMidi(84,1,true)==61,"Harp chord mapping and octave shift");
    require(harpFromMidi(72,1,false)==72,"Chromatic harp mode");
    for(double rate:{44100.0,48000.0,96000.0}) {
        Engine e; e.prepare(rate);
        require(e.next()==0,"Initial silence");
        e.pluck(69,1); double early=0,late=0;
        for(int i=0;i<static_cast<int>(rate*4);++i) {
            const float s=e.next(); require(std::isfinite(s) && std::abs(s)<=1,"Stable bounded output");
            if(i<rate/4) early+=s*s;
            if(i>rate*3.75) late+=s*s;
        }
        require(early>1 && late<early*0.03,"Audible pluck with decaying envelope");
        e.reset(); require(e.next()==0,"Panic silence");
        e.chord(0,0,true); double energy=0;
        for(int i=0;i<rate/2;++i) {const float s=e.next(); energy+=s*s;}
        require(energy>1,"Audible chord organ");
        e.chord(0,0,false);
        for(int i=0;i<rate*3;++i) e.next();
        require(std::abs(e.next())<0.00001,"Chord release");
        for(int n=0;n<128;++n) e.pluck(n);
        for(int i=0;i<10000;++i) require(std::isfinite(e.next()),"Voice stealing stability");
        Engine muted; muted.harpGain=0; muted.prepare(rate); muted.pluck(69);
        for(int i=0;i<10000;++i) require(muted.next()==0,"Independent harp mute");
        muted.organGain=0; muted.prepare(rate); muted.chord(0,0,true);
        for(int i=0;i<10000;++i) require(muted.next()==0,"Independent organ mute");
        for(int mode=0;mode<4;++mode) {
            Engine voice; voice.organSound=mode; voice.harpSound=mode;
            voice.prepare(rate); voice.pluck(72); voice.chord(6,2,true);
            for(int i=0;i<10000;++i) require(std::isfinite(voice.next()),"Voice mode stability");
        }
        Engine organ; organ.vibrato=0; organ.prepare(rate); organ.chord(0,0,true);
        for(int i=0;i<rate/2;++i) organ.next();
        double lowSin=0,lowCos=0,raisedSin=0,raisedCos=0;
        for(int i=0;i<rate;++i) {
            const double sample=organ.next(),t=i/rate;
            lowSin+=sample*std::sin(2*pi*hz(36)*t); lowCos+=sample*std::cos(2*pi*hz(36)*t);
            raisedSin+=sample*std::sin(2*pi*hz(48)*t); raisedCos+=sample*std::cos(2*pi*hz(48)*t);
        }
        require(raisedSin*raisedSin+raisedCos*raisedCos>10*(lowSin*lowSin+lowCos*lowCos),"Organ default is one octave above the v1 register");
        StereoDelay delay; delay.milliseconds=100; delay.feedback=0; delay.mix=1; delay.prepare(rate);
        float maximumLeft=0,maximumRight=0;
        for(int i=0;i<static_cast<int>(rate*0.2);++i) {
            float l=i==0?1.0f:0.0f,r=l; delay.process(l,r);
            if(i>rate*0.09 && i<rate*0.12) { maximumLeft=std::max(maximumLeft,std::abs(l)); maximumRight=std::max(maximumRight,std::abs(r)); }
        }
        // Fractional interpolation can split a unit impulse across two half-height samples.
        require(maximumLeft>0.45f && maximumRight>0.45f,"Stereo delay returns its input after the selected time");
        delay.reset(); float l=0,r=0; delay.process(l,r); require(l==0 && r==0,"Delay reset silence");
    }
    std::cout<<"Engine checks passed at 44.1, 48 and 96 kHz\n";
}
