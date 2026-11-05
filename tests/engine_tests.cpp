#include "Engine.h"
#include <cstdlib>
#include <iostream>
void require(bool condition,const char* message) { if(!condition) { std::cerr<<message<<'\n'; std::exit(1); } }
int main() {
    using namespace chordharp;
    require(chordNote(0,0,0)==48 && chordNote(0,0,1)==52 && chordNote(0,0,3)==60,"Major voicing");
    require(chordNote(0,1,1)==51 && chordNote(0,2,3)==58,"Minor/seventh voicing");
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
    }
    std::cout<<"Engine checks passed at 44.1, 48 and 96 kHz\n";
}
