#pragma once
#include <memory>
#include <vector>

namespace RubberBand
{
class RubberBandLiveShifter;
}

// Adapts Rubber Band's fixed live blocks to arbitrary host block sizes.
// All buffers are allocated in prepare(); process() does not allocate.
class PitchShifter
{
  public:
    PitchShifter();
    ~PitchShifter();
    PitchShifter(PitchShifter &&) noexcept;
    PitchShifter &operator=(PitchShifter &&) noexcept;
    PitchShifter(const PitchShifter &) = delete;
    PitchShifter &operator=(const PitchShifter &) = delete;
    void prepare(double sampleRate, int maxBlockSize, int numChannels = 1);
    void process(const float *const *input, float *const *output, int numSamples, float pitchRatio,
                 bool preserveFormants = false);
    int getNumChannels() const { return static_cast<int>(inputBlocks.size()); }
    void process(const float *input, float *output, int numSamples, float pitchRatio,
                 bool preserveFormants = false);
    void reset();
    int getLatencySamples() const { return latencySamples; }

  private:
    std::unique_ptr<RubberBand::RubberBandLiveShifter> engine;
    std::vector<std::vector<float>> inputBlocks, outputBlocks;
    std::vector<const float *> inputPointers;
    std::vector<float *> outputPointers;
    // Rubber Band applies downward scales at its output resampler, ~40 ms earlier in
    // the source than upward scales. Downward ratios are therefore taken from this
    // history, delayed so both directions land on the audio they were computed for.
    std::vector<float> ratioHistory;
    int historyPosition = 0, downwardDelay = 0;
    int position = 0, latencySamples = 0;
};
