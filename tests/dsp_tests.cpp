#include "dsp/TransposeEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

static thread_local bool countAllocations=false;
static thread_local int allocations=0;
void* operator new(std::size_t n) {
    if(countAllocations) ++allocations;
    if(void* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

constexpr double pi=3.14159265358979323846;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
using Audio=std::vector<std::vector<float>>;
Audio tone(double rate,int count,int channels,double hz) {
    Audio result(channels,std::vector<float>(count));
    for(int c=0;c<channels;++c) for(int i=0;i<count;++i) result[c][i]=float(.25*std::sin(2*pi*hz*i/rate));
    return result;
}
Audio run(bat::TransposeEngine& engine,const Audio& input,const std::vector<int>& blocks,bool inPlace=false) {
    auto output=input;
    int offset=0,b=0;
    while(offset<int(input[0].size())) {
        int n=std::min(blocks[b++%blocks.size()],int(input[0].size())-offset);
        const float* ins[2]{}; float* outs[2]{};
        for(int c=0;c<int(input.size());++c) { ins[c]=(inPlace?output[c]:input[c]).data()+offset; outs[c]=output[c].data()+offset; }
        countAllocations=true;
        engine.process(ins,outs,n);
        countAllocations=false;
        offset+=n;
    }
    return output;
}
void testDryAndBypass() {
    for(double rate:{44100.,48000.,96000.,192000.}) for(int channels:{1,2}) {
        bat::TransposeEngine engine; engine.prepare(rate,channels);
        auto input=tone(rate,int(rate/4),channels,313);
        auto output=run(engine,input,{1,32,7,127,16,511});
        for(int c=0;c<channels;++c) for(int i=engine.latency();i<int(input[c].size());++i)
            require(output[c][i]==input[c][i-engine.latency()],"Zero is not exact delayed dry");
        engine.setSemitones(-7); engine.setBypassed(true); engine.reset();
        output=run(engine,input,{32});
        for(int c=0;c<channels;++c) for(int i=engine.latency();i<int(input[c].size());++i)
            require(output[c][i]==input[c][i-engine.latency()],"Bypass delay mismatch");
    }
}
void testPitch() {
    for(double hz:{55.,82.406889,220.,1318.51}) for(int st=-12;st<=12;++st) {
        bat::TransposeEngine engine; engine.setSemitones(st); engine.prepare(48000,1);
        auto output=run(engine,tone(48000,72000,1,hz),{32})[0];
        for(float x:output) require(std::isfinite(x)&&std::abs(x)<.5f,"Pitch output unstable");
        int crossings=0;
        for(int i=24001;i<72000;++i) if(output[i]>=0&&output[i-1]<0) ++crossings;
        double expected=hz*std::pow(2,st/12.0);
        require(std::abs(crossings-expected)<3.0,"Wrong steady sine pitch");
    }
}
void testBlocksAndStereo() {
    bat::TransposeEngine a,b,c; a.setSemitones(-7); b.setSemitones(-7); c.setSemitones(-7);
    a.prepare(48000,2); b.prepare(48000,2); c.prepare(48000,2);
    auto input=tone(48000,48000,2,83.17);
    for(auto& s:input[1]) s=-s;
    auto one=run(a,input,{32}),two=run(b,input,{1,17,64,511,3}),three=run(c,input,{64},true);
    for(int i=0;i<48000;++i) {
        require(one[0][i]==two[0][i],"Callback partition changed output");
        require(one[0][i]==three[0][i],"In-place processing changed output");
        require(one[0][i]==-one[1][i],"Stereo phase relationship lost");
    }
    a.reset(); auto again=run(a,input,{32}); require(one==again,"Reset is not deterministic");
}
void testAutomationSilenceAndInvalid() {
    bat::TransposeEngine e; e.prepare(48000,2);
    float left[32]{},right[32]{},ol[32]{},orr[32]{};
    const float* in[]={left,right}; float* out[]={ol,orr};
    for(int block=0;block<6000;++block) {
        e.setSemitones((block/31)%25-12); e.setBypassed((block/47)%2!=0);
        for(int i=0;i<32;++i) left[i]=right[i]=block<3000?float(.2*std::sin((block*32+i)*.11)):0;
        if(block==100) left[5]=std::numeric_limits<float>::quiet_NaN();
        if(block==200) right[7]=std::numeric_limits<float>::infinity();
        countAllocations=true; e.process(in,out,32); countAllocations=false;
        for(int i=0;i<32;++i) require(std::isfinite(ol[i])&&std::isfinite(orr[i])&&std::abs(ol[i])<1,"Automation/invalid input unstable");
        if(block>5500) for(int i=0;i<32;++i) require(ol[i]==0&&orr[i]==0,"Tail did not become silent");
    }
    e.process(nullptr,nullptr,0);
    bool threw=false; try { e.prepare(0,1); } catch(const std::invalid_argument&) { threw=true; }
    require(threw,"Invalid sample rate accepted");
}
void testSparseAttacks() {
    for(int st:{-12,-7,-2,2,7,12}) {
        bat::TransposeEngine e; e.setSemitones(st); e.prepare(48000,1);
        Audio x(1,std::vector<float>(48000*5));
        for(int start=6000;start<48000*4;start+=9000)
            for(int j=0;j<2400;++j) x[0][start+j]=float(.2*std::sin(j*2*pi*220/48000)*std::exp(-j/600.0));
        const auto y=run(e,x,{32});
        for(int start=6000;start<48000*4;start+=9000) {
            float peak=0;
            for(int j=0;j<4200;++j) peak=std::max(peak,std::abs(y[0][start+j]));
            if(peak<=.025f) std::cerr<<"Sparse st="<<st<<" start="<<start<<" peak="<<peak<<"\n";
            require(peak>.025f,"Sparse attack lost or delayed beyond 87.5 ms");
        }
        for(int i=48000*4+4000;i<48000*5;++i) require(y[0][i]==0,"Old attack reappeared after silence");
    }
}
int main() try {
    testDryAndBypass(); std::cout<<"PASS exact dry/bypass at 4 sample rates, mono/stereo\n";
    testPitch(); std::cout<<"PASS 100 sine pitch cases, all semitones, 55..1319 Hz\n";
    testBlocksAndStereo(); std::cout<<"PASS block invariance, in-place, stereo phase, reset\n";
    testAutomationSilenceAndInvalid(); std::cout<<"PASS automation, silence/tail, NaN/Inf, input validation\n";
    testSparseAttacks(); std::cout<<"PASS sparse attacks and no stale audio after silence\n";
    require(allocations==0,"Heap allocation in audio callback");
    std::cout<<"PASS zero C++ heap allocations in tested callbacks\n";
    return 0;
} catch(const std::exception& e) { countAllocations=false; std::cerr<<e.what()<<'\n'; return 1; }
