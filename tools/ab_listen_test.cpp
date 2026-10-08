#include "dsp/PitchShifter.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdio>
#include <vector>

static float rms(const float* data, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    return static_cast<float>(std::sqrt(sum / std::max(1, n)));
}

static float correlation(const float* a, const float* b, int n)
{
    double num = 0.0, da = 0.0, db = 0.0;
    for (int i = 0; i < n; ++i)
    {
        num += static_cast<double>(a[i]) * static_cast<double>(b[i]);
        da += static_cast<double>(a[i]) * static_cast<double>(a[i]);
        db += static_cast<double>(b[i]) * static_cast<double>(b[i]);
    }
    const double den = std::sqrt(da * db);
    return den > 0.0 ? static_cast<float>(num / den) : 0.0f;
}

static bool writeWav(const juce::File& file, const float* mono, int numSamples, double sampleRate)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(new juce::FileOutputStream(file), sampleRate, 1, 24, {}, 0));
    if (writer == nullptr)
        return false;

    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.copyFrom(0, 0, mono, numSamples);
    return writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
}

static void processRatio(PitchShifter& shifter, const float* input, float* output, int n, float ratio)
{
    shifter.reset();
    constexpr int block = 256;
    for (int offset = 0; offset < n; offset += block)
    {
        const int count = std::min(block, n - offset);
        shifter.process(input + offset, output + offset, count, ratio);
    }
}

int main(int argc, char** argv)
{
    const juce::File inFile(argc > 1 ? argv[1] : "/home/neckles/Music/girl.wav");
    const juce::File outDir(argc > 2 ? argv[2] : "ab_out");
    const double maxSeconds = argc > 3 ? juce::String(argv[3]).getDoubleValue() : 12.0;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(inFile));
    if (reader == nullptr)
    {
        std::fprintf(stderr, "failed to open %s\n", inFile.getFullPathName().toRawUTF8());
        return 1;
    }

    const int totalAvailable = static_cast<int>(reader->lengthInSamples);
    const int numSamples = std::min(totalAvailable, static_cast<int>(maxSeconds * reader->sampleRate));
    juce::AudioBuffer<float> stereo(static_cast<int>(reader->numChannels), numSamples);
    reader->read(&stereo, 0, numSamples, 0, true, true);

    std::vector<float> dry(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
    {
        float mid = stereo.getSample(0, i);
        if (stereo.getNumChannels() > 1)
            mid = 0.5f * (mid + stereo.getSample(1, i));
        dry[static_cast<size_t>(i)] = mid;
    }

    outDir.createDirectory();

    PitchShifter shifter;
    shifter.prepare(reader->sampleRate, 512);

    std::vector<float> unity(static_cast<size_t>(numSamples), 0.0f);
    std::vector<float> plus50(static_cast<size_t>(numSamples), 0.0f);
    processRatio(shifter, dry.data(), unity.data(), numSamples, 1.0f);
    processRatio(shifter, dry.data(), plus50.data(), numSamples, std::pow(2.0f, 50.0f / 1200.0f));

    const int skip = std::min(4096, numSamples / 4);
    const int compare = numSamples - skip;
    std::printf("input=%s samples=%d sr=%.0f\n", inFile.getFileName().toRawUTF8(), numSamples, reader->sampleRate);
    std::printf("dryRms=%.5f unityRms=%.5f plus50Rms=%.5f\n",
                rms(dry.data() + skip, compare),
                rms(unity.data() + skip, compare),
                rms(plus50.data() + skip, compare));
    std::printf("corr(dry,unity)=%.4f corr(dry,plus50)=%.4f\n",
                correlation(dry.data() + skip, unity.data() + skip, compare),
                correlation(dry.data() + skip, plus50.data() + skip, compare));

    const auto dryOut = outDir.getChildFile("01_dry.wav");
    const auto unityOut = outDir.getChildFile("02_unity.wav");
    const auto plusOut = outDir.getChildFile("03_plus50c.wav");
    if (!writeWav(dryOut, dry.data(), numSamples, reader->sampleRate)
        || !writeWav(unityOut, unity.data(), numSamples, reader->sampleRate)
        || !writeWav(plusOut, plus50.data(), numSamples, reader->sampleRate))
    {
        std::fprintf(stderr, "failed writing WAVs to %s\n", outDir.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf("wrote:\n  %s\n  %s\n  %s\n",
                dryOut.getFullPathName().toRawUTF8(),
                unityOut.getFullPathName().toRawUTF8(),
                plusOut.getFullPathName().toRawUTF8());
    return 0;
}
