#include "dsp/PitchShifter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static bool run(double sr, float ratio, int blockSize, bool formants)
{
    const int total = static_cast<int>(sr * 1.5), skip = static_cast<int>(sr * .5);
    std::vector<float> in(total), out(total);
    for (int i = 0; i < total; ++i)
        in[i] = .4f * std::sin(2.0 * 3.141592653589793 * 220.0 * i / sr);
    PitchShifter shifter;
    shifter.prepare(sr, blockSize);
    for (int i = 0; i < total; i += blockSize)
        shifter.process(in.data() + i, out.data() + i, std::min(blockSize, total - i), ratio, formants);
    double energy = 0;
    int crossings = 0;
    bool finite = true;
    for (int i = skip; i < total; ++i)
    {
        finite &= std::isfinite(out[i]);
        energy += out[i] * out[i];
        if (out[i - 1] <= 0 && out[i] > 0)
            ++crossings;
    }
    const double rms = std::sqrt(energy / (total - skip)), hz = crossings * sr / (total - skip);
    const bool pass = finite && rms > .08 && rms < .7 && std::abs(hz - 220 * ratio) < 4;
    std::printf("%s sr=%.0f ratio=%.3f block=%d formants=%d rms=%.4f hz=%.1f latency=%d\n",
                pass ? "PASS" : "FAIL", sr, ratio, blockSize, formants, rms, hz, shifter.getLatencySamples());
    shifter.reset();
    std::vector<float> silence(8192, 0), resetOut(8192);
    shifter.process(silence.data(), resetOut.data(), 8192, 1);
    for (auto sample : resetOut)
        if (!std::isfinite(sample) || std::abs(sample) > 1.e-6f)
            return false;
    return pass;
}
static bool stereoImage()
{
    constexpr int total = 48000;
    std::vector<float> left(total), right(total), outLeft(total), outRight(total), chunkedLeft(total),
        chunkedRight(total);
    for (int i = 0; i < total; ++i)
    {
        const double phase = i * 2.0 * 3.141592653589793 * 227.0 / 48000.0;
        left[i] = .2f * std::sin(phase) + .08f * std::sin(2 * phase) + .04f * std::sin(3 * phase);
        // A centred vocal with quiet stereo room content, rather than identical channels.
        right[i] = .97f * left[i] + .025f * std::sin(phase - .7) + .008f * std::sin(i * .071);
    }
    PitchShifter one, many;
    one.prepare(48000, total, 2);
    many.prepare(48000, 127, 2);
    const float *input[] = {left.data(), right.data()};
    float *output[] = {outLeft.data(), outRight.data()};
    one.process(input, output, total, 1.04f, true);
    for (int i = 0; i < total; i += 127)
    {
        const float *in[] = {left.data() + i, right.data() + i};
        float *out[] = {chunkedLeft.data() + i, chunkedRight.data() + i};
        many.process(in, out, std::min(127, total - i), 1.04f, true);
    }
    double mid = 0, side = 0, dryMid = 0, drySide = 0;
    for (int i = total / 2; i < total; ++i)
    {
        if (!std::isfinite(outLeft[i]) || !std::isfinite(outRight[i]) ||
            std::abs(outLeft[i] - chunkedLeft[i]) > 1.e-6f ||
            std::abs(outRight[i] - chunkedRight[i]) > 1.e-6f)
            return false;
        mid += std::pow((outLeft[i] + outRight[i]) * .5f, 2);
        side += std::pow((outLeft[i] - outRight[i]) * .5f, 2);
        dryMid += std::pow((left[i] + right[i]) * .5f, 2);
        drySide += std::pow((left[i] - right[i]) * .5f, 2);
    }
    const double wideningDb = 10 * std::log10((side / mid) / (drySide / dryMid));
    const bool pass = mid > 0 && side > 0 && wideningDb < 3.0;
    std::printf("%s stereo image: side/mid change %.2f dB, block-invariant linked channels\n",
                pass ? "PASS" : "FAIL", wideningDb);
    return pass;
}
int main()
{
    bool pass = stereoImage();
    for (double sr : {44100., 48000., 96000.})
        for (float ratio : {.5f, 1.f, 1.059463f, 2.f})
            for (int block : {1, 127, 512, 1023})
                pass = run(sr, ratio, block, false) && pass;
    pass = run(48000, 1.059463f, 127, true) && pass;
    // Block boundaries must not alter a constant-ratio stream.
    std::vector<float> in(10000), a(10000), b(10000);
    for (int i = 0; i < 10000; ++i)
        in[i] = .3f * std::sin(i * .017);
    PitchShifter one, many;
    one.prepare(48000, 10000);
    many.prepare(48000, 127);
    one.process(in.data(), a.data(), 10000, 1.1f);
    for (int i = 0; i < 10000; i += 127)
        many.process(in.data() + i, b.data() + i, std::min(127, 10000 - i), 1.1f);
    for (int i = 0; i < 10000; ++i)
        pass = std::abs(a[i] - b[i]) < 1.e-6f && pass;
    // The adapter also supports in-place processing.
    many.reset();
    b = in;
    many.process(b.data(), b.data(), 10000, 1.1f);
    for (int i = 0; i < 10000; ++i)
        pass = std::abs(a[i] - b[i]) < 1.e-6f && pass;
    return pass ? 0 : 1;
}
