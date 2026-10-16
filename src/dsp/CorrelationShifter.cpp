#include "dsp/CorrelationShifter.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace bat {
namespace { constexpr double pi=3.14159265358979323846; }
void CorrelationShifter::prepare(double sr,int nc,float maxHistoryMs) {
    channels=nc;
    fadeLength=std::max(32,int(sr*.004));
    correlationLength=std::max(64,int(sr*.006));
    // Includes one full low-E power-chord period (~24.3 ms) plus the matching
    // window. A shorter search can lock to the fifth and detune the root.
    baseMaxDelay=int(sr*maxHistoryMs/1000); searchSpan=int(sr*.025);
    nominalDelay=int(sr*.012);
    attackFade=std::max(16,int(sr*.001)); attackHold=int(sr*.02);
    fastCoeff=1-std::exp(-1/(sr*.0005)); slowCoeff=1-std::exp(-1/(sr*.02));
    int size=1; while(size<int(sr*.2)) size*=2; mask=size-1;
    for(auto& r:ring) r.assign(size,0);
    kernels.resize(25*phases*taps);
    for(int s=-12;s<=12;++s) {
        double speed=std::pow(2.,s/12.);
        double cutoff=speed>1?.94/speed:1.;
        for(int p=0;p<phases;++p) {
            double sum=0;
            for(int k=0;k<taps;++k) {
                double x=k-(taps/2-1)-p/double(phases);
                double sinc=std::abs(x)<1e-10?cutoff:std::sin(pi*cutoff*x)/(pi*x);
                double window=.42+.5*std::cos(pi*x/(taps/2))+.08*std::cos(2*pi*x/(taps/2));
                float v=float(sinc*window);
                kernels[((s+12)*phases+p)*taps+k]=v; sum+=v;
            }
            for(int k=0;k<taps;++k) kernels[((s+12)*phases+p)*taps+k]/=float(sum);
        }
    }
    setSemitones(st); reset();
}
void CorrelationShifter::reset() {
    for(auto& r:ring) std::fill(r.begin(),r.end(),0.0f);
    written=0; head=-nominalDelay; nextHead=head; fade=-1;
    triggerDelay=maxDelay;
    fastEnergy=slowEnergy=0; attackArmed=true; attackPending=false; attackCounter=0; fadeTotal=fadeLength;
}
void CorrelationShifter::setSemitones(int semitones) noexcept {
    st=std::clamp(semitones,-12,12); ratio=std::pow(2.,st/12.);
    minDelay=int(std::ceil(ratio*std::max(fadeLength,correlationLength)))+taps+8;
    maxDelay=std::max(baseMaxDelay,minDelay+searchSpan);
    triggerDelay=maxDelay;
}
float CorrelationShifter::fastRead(int c,double p) const noexcept {
    auto i=static_cast<long long>(std::floor(p)); float f=float(p-i);
    float a=ring[c][size_t(i)&mask],b=ring[c][size_t(i+1)&mask];
    return a+(b-a)*f;
}
float CorrelationShifter::read(int c,double p) const noexcept {
    auto i=static_cast<long long>(std::floor(p));
    int phase=std::clamp(int((p-i)*phases),0,phases-1);
    const float* kernel=kernels.data()+((st+12)*phases+phase)*taps;
    float result=0;
    for(int k=0;k<taps;++k) result+=ring[c][size_t(i+k-(taps/2-1))&mask]*kernel[k];
    return result;
}
double CorrelationShifter::findHead(double& quality) const noexcept {
    // Candidate windows and the reference are entirely in recorded history.
    const double newest=double(written)-minDelay;
    const double oldest=double(written)-maxDelay;
    // A splice must recover more delay than is consumed during its crossfade.
    // Otherwise +12 st during silence could repeatedly choose a tiny backward
    // step and eventually read unwritten (wrapped) history.
    const double minJump=std::max(fadeLength*.5,fadeLength*std::abs(1-ratio)+32);
    const double lo=ratio<1?head+minJump:oldest;
    const double hi=ratio<1?newest:head-minJump;
    if(hi<=lo) { quality=0; return std::clamp(double(written)-nominalDelay,oldest,newest); }
    auto score=[&](double candidate) {
        double dot=0,aa=1e-18,bb=1e-18;
        for(int j=0;j<correlationLength;j+=4) for(int c=0;c<channels;++c) {
            const float a=fastRead(c,head+j*ratio),b=fastRead(c,candidate+j*ratio);
            dot+=double(a)*b; aa+=double(a)*a; bb+=double(b)*b;
        }
        // Symmetric normalization also penalizes large level mismatches.
        return 2*dot/(aa+bb) - 1e-6*std::abs(candidate-head)/(hi-lo);
    };
    double best=std::clamp(double(written)-nominalDelay,lo,hi),bestScore=-std::numeric_limits<double>::infinity();
    for(double p=lo;p<=hi;p+=8) {
        const double value=score(p);
        if(value>bestScore) { bestScore=value; best=p; }
    }
    const double coarse=best;
    for(double p=std::max(lo,coarse-8);p<=std::min(hi,coarse+8);p+=.5) {
        const double value=score(p);
        if(value>bestScore) { bestScore=value; best=p; }
    }
    quality=bestScore;
    return best;
}
void CorrelationShifter::process(const float* const* in,float* const* out,int n) noexcept {
    for(int i=0;i<n;++i) {
        double power=0;
        for(int c=0;c<channels;++c) { ring[c][size_t(written)&mask]=in[c][i]; power+=double(in[c][i])*in[c][i]; }
        power/=channels;
        fastEnergy+=(power-fastEnergy)*fastCoeff; slowEnergy+=(power-slowEnergy)*slowCoeff;
        if(attackCounter>0) --attackCounter;
        if(fastEnergy<slowEnergy*1.5) attackArmed=true;
        if(attackArmed&&attackCounter==0&&fastEnergy>1e-5&&fastEnergy>slowEnergy*4) {
            attackArmed=false; attackCounter=attackHold; attackPending=true;
        }
        const double delay=double(written)-head;
        // Re-anchor a fresh pick attack near the newest safe history. This keeps
        // a waveform match from skipping across a quiet-to-loud transition.
        // Both channels share the detector and the entire crossfade decision.
        if(fade<0&&attackPending&&ratio!=1) {
            nextHead=double(written)-minDelay; fade=0; fadeTotal=attackFade;
            attackPending=false; triggerDelay=maxDelay;
        } else if(fade<0 && ((ratio<1&&delay>=triggerDelay)||(ratio>1&&delay<=minDelay))) {
            double quality=0;
            nextHead=findHead(quality);
            if(ratio<1) triggerDelay=quality>.98?std::min(maxDelay,minDelay+int(std::ceil(nextHead-head))+32):maxDelay;
            fade=0; fadeTotal=fadeLength;
        }
        if(ratio==1) attackPending=false;
        const float blend=fade<0?0:float(.5-.5*std::cos(pi*fade/fadeTotal));
        for(int c=0;c<channels;++c) {
            float a=read(c,head);
            out[c][i]=fade<0?a:a+(read(c,nextHead)-a)*blend;
        }
        head+=ratio;
        if(fade>=0) { nextHead+=ratio; if(++fade>=fadeTotal) { head=nextHead; fade=-1; } }
        ++written;
    }
}
}
