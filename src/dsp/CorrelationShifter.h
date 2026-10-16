#pragma once
#include <array>
#include <vector>

namespace bat {
// Time-domain pitch shifter. Fractional read heads are
// joined at waveform matches, with linked stereo decisions and bandlimited reads.
class CorrelationShifter {
public:
    void prepare(double sampleRate,int channels,float maxHistoryMs=35);
    void reset();
    void setSemitones(int st) noexcept;
    void process(const float* const* in,float* const* out,int count) noexcept;
    int latency() const noexcept { return nominalDelay; }
    int tail() const noexcept { return maxDelay*2; }
private:
    static constexpr int taps=24, phases=128;
    std::array<std::vector<float>,2> ring;
    std::vector<float> kernels;
    int channels=1,mask=0,fadeLength=192,correlationLength=256;
    int nominalDelay=480,maxDelay=1104,minDelay=144,triggerDelay=1104,st=0,fade=-1;
    int baseMaxDelay=1104,searchSpan=1200;
    int fadeTotal=192,attackFade=48,attackHold=960,attackCounter=0;
    double fastEnergy=0,slowEnergy=0,fastCoeff=0,slowCoeff=0;
    bool attackArmed=true,attackPending=false;
    long long written=0;
    double head=0,nextHead=0,ratio=1;
    float fastRead(int channel,double position) const noexcept;
    float read(int channel,double position) const noexcept;
    double findHead(double& quality) const noexcept;
};
}
