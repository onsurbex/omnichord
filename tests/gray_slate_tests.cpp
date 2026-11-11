#include "GraySlate.h"
#include <cstdlib>
#include <iostream>
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    using namespace chordharp;
    SampleAudio pcm;pcm.rate=48000;
    for(int i=0;i<512;++i){const float x=static_cast<float>((i*61)%65536-32768)/32768;pcm.channel[0].push_back(x);pcm.channel[1].push_back(x);}
    const auto original=pcm;pcm.compact(true);
    require(pcm.storageBytes()==512*2 && pcm.frames()==512,"Mono PCM16 must keep one lossless compact channel");
    for(double p=-1;p<514;p+=.13)for(size_t c=0;c<2;++c)require(std::abs(pcm.read(c,p)-original.read(c,p))<1e-6f,"Compact storage must preserve stereo sample interpolation exactly");
    GraySlateBank bank;
    for(int q=0;q<3;++q)for(int r=0;r<12;++r) {
        auto& chord=bank.chords[static_cast<size_t>(q*12+r)];chord.rate=48000;chord.channel[0].assign(2048,(1+q*12+r)*.001f);chord.channel[1]=chord.channel[0];
        for(int i=0;i<12;++i){auto& note=bank.harps[static_cast<size_t>(q*12+r)][static_cast<size_t>(i)];note.rate=48000;note.root=48+r+i;note.channel[0].assign(2048,(1+q*144+r*12+i)*.0001f);note.channel[1]=note.channel[0];}
    }
    for(int q=0;q<3;++q)for(int r=0;r<12;++r) {
        GraySlatePlayer player;player.bank=&bank;player.loop=false;player.prepare(48000,0);player.chord(r,q,true,0);
        for(int i=0;i<100;++i){const auto x=player.next();if(i>50)require(std::abs(x[0]-(1+q*12+r)*.001f)<1e-6f,"Chord changes must route to the corresponding single recording");}
        for(int i=0;i<12;++i){const auto pick=bank.step(r,q,i);require(pick.audio==&bank.harps[static_cast<size_t>(q*12+r)][static_cast<size_t>(i)] && pick.pitch==1,"All twelve recorded harp positions must bypass pitch shifting");require(bank.step(r,q,i+12).pitch==2,"An extra octave must transpose the matching recording");}
    }
    bank.harps[10][11]={};require(bank.step(10,0,11).audio==&bank.harps[10][10] && bank.step(10,0,11).pitch>1,"Missing Bb position must use a nearby recording");
    for(double rate:{44100.,48000.,96000.}) {
        const int latency=rate>=88200?32:16;GraySlatePlayer player;player.prepare(rate,latency);player.chord(0,0,true,0);player.bank=&bank;player.chord(0,0,true,0);
        for(int i=0;i<latency;++i)require(player.next()[0]==0,"Recorded audio must respect the declared plugin latency");
        require(player.next()[0]>0,"A bank arriving after a held chord must start without another note-on");
        player.release=.01f;player.releaseAll();float tail=0;for(int i=0;i<static_cast<int>(rate*.03);++i)tail=player.next()[0];require(tail==0,"Disabling a recorded instrument must release every voice");
    }
    std::cout<<"GraySlate routing, interpolation, transposition, latency and release passed\n";
}
