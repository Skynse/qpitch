#include "dsp/PitchShifter.h"
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

int main()
{
    constexpr double sr = 44100.0;
    constexpr int total = 44100 * 2;
    constexpr float freq = 440.0f;

    std::vector<float> input(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i)
        input[static_cast<size_t>(i)] = std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / static_cast<float>(sr));

    PitchShifter shifter;
    shifter.prepare(sr, 512);

    for (float ratio : { 1.0f, 1.05946f, 0.5f, 2.0f })
    {
        shifter.reset();
        std::vector<float> output(static_cast<size_t>(total), 0.0f);
        constexpr int block = 256;
        for (int offset = 0; offset < total; offset += block)
        {
            const int n = std::min(block, total - offset);
            shifter.process(input.data() + offset, output.data() + offset, n, ratio);
        }

        const int skip = 2048;
        const float inRms = rms(input.data() + skip, total - skip);
        const float outRms = rms(output.data() + skip, total - skip);
        std::printf("ratio=%.4f inRms=%.5f outRms=%.5f peak=%.5f\n",
                    ratio, inRms, outRms,
                    *std::max_element(output.begin() + skip, output.end()));
    }

    return 0;
}
