#pragma once
#include <memory>

namespace bat {
// A single linked polyphonic engine for mono/stereo. All allocation is in prepare.
class TransposeEngine {
public:
    TransposeEngine();
    ~TransposeEngine();
    TransposeEngine(const TransposeEngine&) = delete;
    TransposeEngine& operator=(const TransposeEngine&) = delete;
    void prepare(double sampleRate, int channels);
    void reset();
    void setSemitones(int semitones) noexcept;
    void setBypassed(bool bypassed) noexcept;
    int latency() const noexcept;
    int tail() const noexcept;
    // Arbitrary block lengths, including in-place buffers and zero samples.
    void process(const float* const* input, float* const* output, int frames) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
