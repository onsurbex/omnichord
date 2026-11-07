#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace chordharp {
struct DCBlocker {
    std::array<float,2> input{},output{};float pole=.999f;
    void prepare(double rate) {pole=static_cast<float>(std::exp(-6.283185307179586*10/rate));reset();}
    void reset() {input={};output={};}
    float next(float x,size_t i) {const float y=x-input[i]+pole*output[i];input[i]=x;output[i]=y;return y;}
};
class SynthFilter {
    struct Channel {float ic1=0,ic2=0;};std::array<Channel,2> channel{};
    double rate=96000;float g=.1f,k=1.414f,a1=1,logHz=8,smoothedQ=.707f,coefficient=.01f;
    std::array<float,3> modeWeight{{1,0,0}};
public:
    float cutoff=12000,resonance=.707f;int mode=0;
    void prepare(double sampleRate) {rate=sampleRate;channel={};logHz=std::log(std::clamp(cutoff,20.f,20000.f));smoothedQ=resonance;coefficient=static_cast<float>(1-std::exp(-16/(rate*.02)));modeWeight={};modeWeight[static_cast<size_t>(std::clamp(mode,0,2))]=1;tick();}
    void reset() {channel={};}
    void tick() {
        logHz+=coefficient*(std::log(std::clamp(cutoff,20.f,20000.f))-logHz);smoothedQ+=coefficient*(std::clamp(resonance,.5f,8.f)-smoothedQ);
        g=std::tan(static_cast<float>(3.141592653589793*std::min<double>(std::exp(logHz),rate*.22)/rate));k=1/smoothedQ;a1=1/(1+g*(g+k));
        for(int i=0;i<3;++i) modeWeight[static_cast<size_t>(i)]+=coefficient*((i==std::clamp(mode,0,2)?1.f:0.f)-modeWeight[static_cast<size_t>(i)]);
    }
    float next(float x,size_t index) {
        auto& c=channel[index];const float v1=a1*(c.ic1+g*(x-c.ic2)),v2=c.ic2+g*v1;
        c.ic1=2*v1-c.ic1;c.ic2=2*v2-c.ic2;
        return modeWeight[0]*v2+modeWeight[1]*k*v1+modeWeight[2]*(x-k*v1-v2);
    }
};
class Downsample2 {
    std::array<std::array<float,65>,2> history{};size_t write=0;
    static const std::array<float,65>& coefficients() {
        static const auto h=[] {std::array<float,65> result{};double sum=0;for(int i=0;i<65;++i){const double x=i-32,v=(x==0?.45:std::sin(3.141592653589793*.45*x)/(3.141592653589793*x))*(.54-.46*std::cos(6.283185307179586*i/64));result[static_cast<size_t>(i)]=static_cast<float>(v);sum+=v;}for(auto& v:result)v=static_cast<float>(v/sum);return result;}();return h;
    }
public:
    void reset() {history={};write=0;(void)coefficients();}
    void push(float l,float r) {history[0][write]=l;history[1][write]=r;if(++write==65)write=0;}
    std::array<float,2> output() const {std::array<float,2> result{};const auto& h=coefficients();size_t at=write;for(size_t i=0;i<65;++i){if(at==0)at=65;--at;result[0]+=h[i]*history[0][at];result[1]+=h[i]*history[1][at];}return result;}
};
}
