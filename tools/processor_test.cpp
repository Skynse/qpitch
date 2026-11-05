// Offline checks of the correction loop on a synthetic vibrato voice.
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

static constexpr double sampleRate = 48000.0;

// A harmonic voice ~35 ct sharp of A3 with 5.5 Hz, +/-35 ct vibrato.
static std::vector<float> voice(int total)
{
    std::vector<float> x(static_cast<size_t>(total));
    double phase = 0;
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sampleRate;
        const double cents = 35 + 35 * std::sin(2 * 3.141592653589793 * 5.5 * t);
        phase += 2 * 3.141592653589793 * 220.0 * std::pow(2.0, cents / 1200) / sampleRate;
        x[static_cast<size_t>(i)] = static_cast<float>(.25 * std::sin(phase) + .1 * std::sin(2 * phase) + .05 * std::sin(3 * phase));
    }
    return x;
}

static std::vector<float> render(const std::vector<float>& input, int blockSize,
                                 std::initializer_list<std::pair<const char*, float>> settings)
{
    QPitchAudioProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::mono());
    processor.setBusesLayout(layout);
    auto& state = processor.getValueTreeState();
    for (auto [id, value] : settings)
        state.getParameter(id)->setValueNotifyingHost(state.getParameter(id)->convertTo0to1(value));
    processor.prepareToPlay(sampleRate, blockSize);
    const int latency = processor.getLatencySamples(), total = static_cast<int>(input.size());
    std::vector<float> output(input.size());
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total + latency; pos += blockSize)
    {
        const int count = std::min(blockSize, total + latency - pos);
        juce::AudioBuffer<float> block(1, count);
        for (int i = 0; i < count; ++i)
            block.setSample(0, i, pos + i < total ? input[static_cast<size_t>(pos + i)] : 0.f);
        processor.processBlock(block, midi);
        for (int i = 0; i < count; ++i)
            if (pos + i >= latency)
                output[static_cast<size_t>(pos + i - latency)] = block.getSample(0, i);
    }
    return output;
}

// Fraction of 20 ms analysis frames within maxCents of an equal-tempered note.
static double inTune(const std::vector<float>& audio, double maxCents)
{
    PitchDetector detector;
    detector.prepare(sampleRate, 128);
    detector.setFrequencyRange(55, 1000);
    int frames = 0, good = 0;
    for (size_t pos = static_cast<size_t>(sampleRate * .5); pos + 128 < audio.size(); pos += 128)
    {
        const float hz = detector.detectPitch(audio.data() + pos, 128);
        if (hz <= 0 || detector.getConfidence() < .8f)
            continue;
        const double midi = 69 + 12 * std::log2(hz / 440.0);
        ++frames;
        good += std::abs(midi - std::round(midi)) * 100 <= maxCents;
    }
    return frames > 0 ? static_cast<double>(good) / frames : 0;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    bool pass = true;
    const auto input = voice(static_cast<int>(sampleRate * 3));
    const std::initializer_list<std::pair<const char*, float>> hard {
        { "scale", 0 }, { "retune_speed", 0 }, { "note_transition", 0 } };

    const auto hardTuned = render(input, 512, hard);
    const double dry10 = inTune(input, 10), dry20 = inTune(input, 20);
    const double tuned10 = inTune(hardTuned, 10), tuned20 = inTune(hardTuned, 20);
    std::printf("hard tune: dry %.0f%% / %.0f%%, tuned %.0f%% / %.0f%% of frames within 10 / 20 ct\n",
                dry10 * 100, dry20 * 100, tuned10 * 100, tuned20 * 100);
    // Before the timing fixes this measured 23% / 41%.
    const bool crisp = tuned10 > .5 && tuned20 > .6 && dry10 < .3;
    std::printf("%s hard tune holds a vibrato voice in tune\n", crisp ? "PASS" : "FAIL");
    pass = crisp && pass;

    // Host buffer size must not change the result.
    const auto reference = render(input, 128, hard);
    for (int block : { 256, 1024, 2048 })
    {
        const auto other = render(input, block, hard);
        float worst = 0;
        for (size_t i = 0; i < input.size(); ++i)
            worst = std::max(worst, std::abs(other[i] - reference[i]));
        const bool same = worst < 1.e-5f;
        std::printf("%s block %d matches block 128 (max diff %.2g)\n", same ? "PASS" : "FAIL", block, worst);
        pass = same && pass;
    }

    // Every timing control must audibly change the result.
    const auto base = render(input, 512, { { "scale", 0 }, { "retune_speed", 10 }, { "note_transition", 60 } });
    for (auto [id, value] : std::initializer_list<std::pair<const char*, float>> {
             { "retune_speed", 200 }, { "humanize", 100 }, { "tpain", 100 } })
    {
        const auto changed = render(input, 512, { { "scale", 0 }, { "retune_speed", 10 }, { "note_transition", 60 }, { id, value } });
        double diff = 0, energy = 0;
        for (size_t i = 0; i < input.size(); ++i)
        {
            diff += std::pow(changed[i] - base[i], 2);
            energy += std::pow(base[i], 2);
        }
        const double db = 10 * std::log10(diff / energy);
        const bool audible = db > -30;
        std::printf("%s %s=%.0f changes output (%.1f dB difference)\n", audible ? "PASS" : "FAIL", id, value, db);
        pass = audible && pass;
    }
    return pass ? 0 : 1;
}
