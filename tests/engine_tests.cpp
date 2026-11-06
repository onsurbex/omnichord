#include "Engine.h"
#include "KeyboardMapping.h"
#include "Delay.h"
#include "Tape.h"
#include "ChordGate.h"
#include "Tempo.h"
#include <cstdlib>
#include <iostream>
#include <limits>
void require(bool condition,const char* message) { if(!condition) { std::cerr<<message<<'\n'; std::exit(1); } }
int main() {
    using namespace chordharp;
    for(size_t length:{size_t(5764),size_t(12484),size_t(657604)})
        for(size_t write:{size_t(0),size_t(1),size_t(1764),length-1}) {
            const auto delay=std::nextafter(static_cast<double>(write),std::numeric_limits<double>::infinity());
            const auto position=circularReadPosition(write,delay,length);
            require(position>=0 && position<static_cast<double>(length),"Fractional circular reads must stay in bounds at rounded wrap endpoints");
        }
    require(chordNote(0,0,0)==48 && chordNote(0,0,1)==52 && chordNote(0,0,3)==60,"Major voicing");
    require(chordNote(0,1,1)==51 && chordNote(0,2,3)==58,"Minor/seventh voicing");
    for(int quality=0;quality<4;++quality) for(int col=0;col<12;++col)
        require(chordFromMidi(24+quality*12+col)==quality*12+omnichordRoots[static_cast<size_t>(col)],"Omnichord position-to-MIDI mapping");
    require(chordFromMidi(23)==-1 && chordFromMidi(72)==-1,"Selector MIDI boundaries");
    require(chordFromMidi(24)==1 && chordFromMidi(35)==6,"Major row Db to F#");
    require(chordFromMidi(36)==13 && chordFromMidi(71)==42,"Minor/seventh rows");
    require(harpFromMidi(72,1,true)==49 && harpFromMidi(84,1,true)==61,"Harp chord mapping and octave shift");
    require(harpFromMidi(72,1,false)==72,"Chromatic harp mode");
    require(chordNote(0,3,3)==59 && chordNote(0,2,3)==58,"Major seventh is distinct from dominant seventh");
    const std::u32string_view rows[]{U"QWERTYUIOP",U"ASDFGHJKLÑ",U"ZXCVBNM,.-",U"ASDFGHJKLÑ"};
    for(int q=0;q<4;++q) for(int col=0;col<12;++col) {
        const bool control=col<2; const int key=control?col:col-2;
        require(chordFromComputerKey(rows[q][static_cast<size_t>(key)],control,q==3)==q*12+omnichordRoots[static_cast<size_t>(col)],"Every Mac chord key follows the requested physical order");
    }
    require(chordFromComputerKey(U'ñ',false,true)==42,"Shift + enye selects F#Maj7");
    require(chordFromComputerKey(U'E',true,false)==-1,"Undefined Ctrl shortcuts are not intercepted");
    for(int n=0;n<10;++n) require(harpFromComputerKey(U"1234567890"[n])==n,"Number keys play harp strings");
    require(harpFromComputerKey(U'\'')==10 && harpFromComputerKey(U'¡')==11,"Spanish number-row punctuation completes twelve strings");
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
            for(int quality=0;quality<4;++quality) {
                voice.chord(6,quality,true);
                for(int i=0;i<10000;++i) require(std::isfinite(voice.next()),"Voice mode stability");
            }
        }
        Engine wide; wide.organChorus=0.8f; wide.prepare(rate); wide.chord(0,3,true);
        double stereo=0; for(int i=0;i<rate;++i) { wide.next(); const auto d=wide.leftOutput-wide.rightOutput; stereo+=d*d; }
        require(stereo>0.01,"Organ chorus produces a measurable stereo image");
        Engine mono; mono.organChorus=0; mono.prepare(rate); mono.chord(0,3,true);
        for(int i=0;i<10000;++i) { mono.next(); require(mono.leftOutput==mono.rightOutput,"Zero chorus remains mono"); }
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
    require(chordNote(0,3,3)==59,"Maj7 keeps its major seventh");
    ChordGate gate; require(gate.latest()==-1,"Chord gate starts empty");
    gate.press(0,1); gate.press(48,8); require(gate.latest()==8,"Newest held chord wins");
    gate.release(48); require(gate.latest()==1,"Releasing the newest chord returns to the held chord");
    gate.press(863,1); gate.release(0); require(gate.latest()==1,"GUI and MIDI sources are independent");
    gate.release(863); require(gate.latest()==-1,"The final release closes the chord gate");
    gate.press(864,7); require(gate.latest()==-1,"Invalid sources are ignored");
    gate.press(767,42); gate.clear(); require(gate.latest()==-1,"Panic clears every source");
    for(int chord=0;chord<48;++chord) require(chordFromMidi(midiFromChord(chord))==chord,"GUI and MIDI chord maps agree");
    for(double rate:{44100.0,48000.0,96000.0}) {
        Tape bypass; bypass.noiseLevel=1; bypass.wowAmount=1; bypass.flutterAmount=1; bypass.driveAmount=1; bypass.prepare(rate);
        for(int i=0;i<10000;++i) {
            float l=static_cast<float>(std::sin(i*0.1)),r=l*0.25f; const auto a=l,b=r;
            bypass.process(l,r); require(l==a && r==b,"Disabled tape is an exact stereo bypass");
        }
        Tape silent; silent.enabled=true; silent.noiseLevel=0; silent.prepare(rate);
        for(int i=0;i<10000;++i) { float l=0,r=0; silent.process(l,r); require(l==0 && r==0,"Zero tape noise preserves silence"); }
        const auto noiseEnergy=[&](float level) {
            Tape t; t.enabled=true; t.noiseLevel=level; t.prepare(rate); double sum=0;
            for(int i=0;i<rate/2;++i) { float l=0,r=0; t.process(l,r); sum+=l*l+r*r; }
            t.reset(); float l=0,r=0; t.process(l,r,false); require(l==0 && r==0,"Tape reset and panic suppress hiss");
            return sum;
        };
        const auto quiet=noiseEnergy(0.1f),loud=noiseEnergy(1);
        require(quiet>0.00001 && loud>quiet*90 && loud<quiet*110,"Noise knob controls the level of stereo hiss");
        const auto harmonic=[&](float drive) {
            Tape t; t.enabled=true; t.driveAmount=drive; t.wowAmount=t.flutterAmount=0; t.prepare(rate);
            double sine=0,cosine=0;
            for(int i=0;i<rate;++i) {
                float l=0.25f*static_cast<float>(std::sin(2*pi*500*i/rate)),r=l; t.process(l,r);
                require(l==r,"Common tape speed preserves a mono source without noise");
                if(i>=rate/2) { sine+=l*std::sin(2*pi*1500*i/rate); cosine+=l*std::cos(2*pi*1500*i/rate); }
            }
            return sine*sine+cosine*cosine;
        };
        require(harmonic(1)>harmonic(0)*100+1,"Drive adds measurable saturation harmonics");
        const auto speedVariation=[&](bool flutter) {
            Tape t; t.enabled=true; t.driveAmount=0; t.wowAmount=flutter?0.0f:1.0f; t.flutterAmount=flutter?1.0f:0.0f; t.prepare(rate);
            float previous=0; double crossing=-1,minimum=1e9,maximum=0;
            for(int i=0;i<rate*2;++i) {
                float l=static_cast<float>(std::sin(2*pi*1000*i/rate)),r=l; t.process(l,r);
                if(i>rate/2 && previous<=0 && l>0) {
                    const double current=i-static_cast<double>(l)/(l-previous);
                    if(crossing>=0) { minimum=std::min(minimum,current-crossing); maximum=std::max(maximum,current-crossing); }
                    crossing=current;
                }
                previous=l;
            }
            return (maximum-minimum)/(rate/1000);
        };
        require(speedVariation(false)>0.002,"Wow changes pitch slowly");
        require(speedVariation(true)>0.002,"Flutter changes pitch rapidly");
        Tape extreme; extreme.enabled=true; extreme.noiseLevel=extreme.wowAmount=extreme.flutterAmount=extreme.driveAmount=1; extreme.prepare(rate);
        for(int i=0;i<rate;++i) { float l=16*static_cast<float>(std::sin(i*0.23)),r=-l; extreme.process(l,r); require(std::isfinite(l) && std::isfinite(r) && std::abs(l)<2 && std::abs(r)<2,"Tape remains stable at maximum settings"); }
    }
    require(std::abs(syncedDelayMs(120,4)-500)<.001,"Quarter-note delay at 120 BPM");
    require(std::abs(syncedDelayMs(120,8)-166.6666667)<.001,"Eighth-note triplet timing");
    require(std::abs(syncedDelayMs(90,9)-500)<.001,"Dotted eighth timing");
    require(syncedDelayMs(20,0)==12000,"Whole note at the lowest tempo fits the delay buffer");
    for(double rate:{44100.0,48000.0,96000.0}) {
        for(int model=0;model<=10;++model) {
            Engine organ; organ.organModel=model; organ.prepare(rate); organ.chord(9,3,true); double energy=0;
            for(int i=0;i<rate/2;++i) { const float sample=organ.next(); require(std::isfinite(sample) && std::abs(sample)<=1,"All organ models remain finite and bounded"); energy+=sample*sample; }
            require(energy>.1,"Every organ registration must be audible");
            organ.chord(9,3,false);for(int i=0;i<rate*2;++i) organ.next();require(std::abs(organ.next())<.00001,"All organ models release after note off");
        }
        const auto subEnergy=[&](int model) {
            Engine organ;organ.organModel=model;organ.vibrato=0;organ.organChorus=organ.organRotary=0;organ.prepare(rate);organ.chord(9,0,true);
            double sine=0,cosine=0;
            for(int i=0;i<rate*2;++i) { const double y=organ.next();if(i>=rate){sine+=y*std::sin(2*pi*110*i/rate);cosine+=y*std::cos(2*pi*110*i/rate);} }
            return sine*sine+cosine*cosine;
        };
        require(subEnergy(8)>subEnergy(4)*10,"Tonewheel drawbars add a genuine 16-foot sub-octave rank");
        StereoDelay clock;clock.synced=true;clock.milliseconds=static_cast<float>(syncedDelayMs(120,4));clock.feedback=0;clock.mix=1;clock.prepare(rate);
        float peak=0;int peakAt=0;
        for(int i=0;i<rate*.7;++i){float l=i==0?1.0f:0,r=l;clock.process(l,r);if(i>1){require(l==r,"Synced stereo echoes share the exact beat duration");if(l>peak){peak=l;peakAt=i;}}}
        require(std::abs(peakAt-rate*.5)<2 && peak>.9,"The synced echo occurs at the expected beat");
    }
    std::cout<<"Engine checks passed at 44.1, 48 and 96 kHz\n";
}
