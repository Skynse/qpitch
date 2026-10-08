#include "PitchShifter.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <rubberband/RubberBandLiveShifter.h>

PitchShifter::PitchShifter() = default;
PitchShifter::~PitchShifter() = default;
PitchShifter::PitchShifter(PitchShifter &&) noexcept = default;
PitchShifter &PitchShifter::operator=(PitchShifter &&) noexcept = default;

void PitchShifter::prepare(double sampleRate, int, int numChannels)
{
    using Live = RubberBand::RubberBandLiveShifter;
    numChannels = std::max(1, numChannels);
    engine = std::make_unique<Live>(static_cast<size_t>(sampleRate), static_cast<size_t>(numChannels),
                                    Live::OptionWindowShort | Live::OptionChannelsTogether);
    const auto blockSize = engine->getBlockSize();
    inputBlocks.assign(numChannels, std::vector<float>(blockSize, 0.0f));
    outputBlocks.assign(numChannels, std::vector<float>(blockSize, 0.0f));
    inputPointers.resize(numChannels);
    outputPointers.resize(numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        inputPointers[ch] = inputBlocks[ch].data();
        outputPointers[ch] = outputBlocks[ch].data();
    }
    // One adapter block plus the live engine's initial delay.
    latencySamples = static_cast<int>(blockSize + engine->getStartDelay());
    position = 0;
}

void PitchShifter::reset()
{
    if (engine)
    {
        engine->reset();
        engine->setPitchScale(1.0);
    }
    for (auto &block : inputBlocks)
        std::fill(block.begin(), block.end(), 0.0f);
    for (auto &block : outputBlocks)
        std::fill(block.begin(), block.end(), 0.0f);
    position = 0;
}

void PitchShifter::process(const float *input, float *output, int n, float ratio, bool preserveFormants)
{
    assert(getNumChannels() <= 1);
    const float *inputs[] = {input};
    float *outputs[] = {output};
    process(inputs, outputs, n, ratio, preserveFormants);
}

void PitchShifter::process(const float *const *input, float *const *output, int n, float ratio,
                           bool preserveFormants)
{
    if (!engine)
    {
        std::copy(input[0], input[0] + n, output[0]);
        return;
    }
    const double scale = std::isfinite(ratio) ? std::clamp(ratio, .5f, 2.f) : 1.f;
    using Live = RubberBand::RubberBandLiveShifter;
    engine->setFormantOption(preserveFormants ? Live::OptionFormantPreserved : Live::OptionFormantShifted);
    for (int i = 0; i < n; ++i)
    {
        // All channels share one analysis clock and Rubber Band's linked phase decisions.
        for (int ch = 0; ch < getNumChannels(); ++ch)
        {
            inputBlocks[ch][position] = input[ch][i];
            output[ch][i] = outputBlocks[ch][position];
        }
        if (++position == static_cast<int>(inputBlocks.front().size()))
        {
            engine->setPitchScale(scale);
            engine->shift(inputPointers.data(), outputPointers.data());
            position = 0;
        }
    }
}
