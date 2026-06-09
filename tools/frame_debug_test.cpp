#include "dsp/PitchShifter.h"
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    constexpr double sr = 44100.0;
    constexpr int N = 1024;
    constexpr int hop = 256;
    constexpr int latency = N - hop;

    PitchShifter shifter;
    shifter.prepare(sr, 512);

    std::vector<float> input(static_cast<size_t>(N * 3), 0.0f);
    for (int i = 0; i < N * 3; ++i)
        input[static_cast<size_t>(i)] = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / static_cast<float>(sr));

    std::vector<float> output(input.size(), 0.0f);
    shifter.process(input.data(), output.data(), static_cast<int>(input.size()), 1.0f);

    float peak = 0.0f;
    for (size_t i = latency; i < output.size(); ++i)
        peak = std::max(peak, std::abs(output[i]));
    std::printf("after %zu samples, peak after latency = %.6f\n", output.size(), peak);
    return 0;
}
