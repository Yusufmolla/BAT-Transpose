#include "dsp/TransposeEngine.h"
#include "dsp/CorrelationShifter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
#if defined(_M_X64) || defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace bat {
namespace {
struct DenormalGuard {
#if defined(_M_X64) || defined(__SSE__)
    unsigned old = _mm_getcsr();
    DenormalGuard() { _mm_setcsr(old | 0x8040); }
    ~DenormalGuard() { _mm_setcsr(old); }
#elif defined(__aarch64__)
    // Apple Silicon/Linux ARM: restore the host's floating-point mode on return.
    unsigned long long old=0;
    DenormalGuard() {
        asm volatile("mrs %0, fpcr" : "=r"(old));
        const auto flush=old | (1ULL<<24);
        asm volatile("msr fpcr, %0" :: "r"(flush));
    }
    ~DenormalGuard() { asm volatile("msr fpcr, %0" :: "r"(old)); }
#endif
};
constexpr int chunkSize = 32;
}
struct TransposeEngine::Impl {
    CorrelationShifter correlation;
    std::array<std::array<float, chunkSize>, 2> in{}, wet{}, dry{};
    std::array<std::vector<float>, 2> delay;
    int channels = 0, delaySamples = 0, delayPosition = 0, tailSamples = 0;
    int semitones = 0;
    bool bypassed = false;
    float mix = 0, mixStep = 1.0f/240.0f;
};
TransposeEngine::TransposeEngine() : impl(std::make_unique<Impl>()) {}
TransposeEngine::~TransposeEngine() = default;
void TransposeEngine::prepare(double rate, int channels) {
    if (!std::isfinite(rate) || rate < 8000 || rate > 384000 || channels < 1 || channels > 2)
        throw std::invalid_argument("Unsupported DSP configuration");
    auto& s = *impl;
    s.channels = channels;
    s.correlation.prepare(rate, channels);
    s.correlation.setSemitones(s.semitones);
    s.delaySamples = s.correlation.latency();
    s.tailSamples = s.correlation.tail();
    s.mixStep = float(1.0/(rate*0.005));
    for (auto& d : s.delay) d.assign(s.delaySamples + 1, 0.0f);
    reset();
}
void TransposeEngine::reset() {
    auto& s = *impl;
    if (s.channels == 0) return;
    s.correlation.reset();
    for (auto& d : s.delay) std::fill(d.begin(), d.end(), 0.0f);
    s.delayPosition = 0;
    s.mix = (s.bypassed || s.semitones == 0) ? 0.0f : 1.0f;
}
void TransposeEngine::setSemitones(int semitones) noexcept {
    auto& s = *impl;
    semitones = std::clamp(semitones, -12, 12);
    if (s.semitones == semitones) return;
    s.semitones = semitones;
    s.correlation.setSemitones(semitones);
}
void TransposeEngine::setBypassed(bool bypassed) noexcept { impl->bypassed = bypassed; }
int TransposeEngine::latency() const noexcept { return impl->delaySamples; }
int TransposeEngine::tail() const noexcept { return impl->tailSamples; }
void TransposeEngine::process(const float* const* input, float* const* output, int frames) noexcept {
    DenormalGuard guard;
    auto& s = *impl;
    if (s.channels == 0 || frames <= 0) return;
    const float targetMix = (s.bypassed || s.semitones == 0) ? 0.0f : 1.0f;
    for (int offset = 0; offset < frames; offset += chunkSize) {
        const int n = std::min(chunkSize, frames - offset);
        // Capture every channel first so in-place stereo processing is safe.
        for (int i = 0; i < n; ++i) {
            const int read = (s.delayPosition + 1) % int(s.delay[0].size());
            for (int c = 0; c < s.channels; ++c) {
                float x = input && input[c] ? input[c][offset + i] : 0.0f;
                if (!std::isfinite(x)) x = 0;
                s.in[c][i] = x;
                s.delay[c][s.delayPosition] = x;
                s.dry[c][i] = s.delay[c][read];
            }
            s.delayPosition = read;
        }
        const float* ins[] = {s.in[0].data(), s.in[1].data()};
        float* outs[] = {s.wet[0].data(), s.wet[1].data()};
        // Keep history warm at zero/bypass; constant latency avoids host PDC jumps.
        s.correlation.process(ins, outs, n);
        for (int i = 0; i < n; ++i) {
            s.mix += std::clamp(targetMix - s.mix, -s.mixStep, s.mixStep);
            for (int c = 0; c < s.channels; ++c) {
                float y = s.dry[c][i] + s.mix*(s.wet[c][i] - s.dry[c][i]);
                output[c][offset + i] = std::isfinite(y) ? y : 0.0f;
            }
        }
    }
}
}
