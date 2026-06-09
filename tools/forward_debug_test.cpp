#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    constexpr int N = 1024;
    constexpr int hop = 256;
    juce::dsp::FFT fft(10);

    std::vector<float> fifo(N, 0.0f);
    std::vector<float> window(N);
    for (int i = 0; i < N; ++i)
        window[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(N));

    for (int i = 0; i < N; ++i)
        fifo[static_cast<size_t>(i)] = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f) * window[static_cast<size_t>(i)];

    std::vector<std::complex<float>> in(N), out(N);
    for (int k = 0; k < N; ++k)
        in[static_cast<size_t>(k)] = { fifo[static_cast<size_t>(k)], 0.0f };

    fft.perform(in.data(), out.data(), false);

    float maxMag2 = 0.0f;
    int maxBin = 0;
    for (int k = 0; k < N / 2 + 1; ++k)
    {
        const float m = 2.0f * std::abs(out[static_cast<size_t>(k)]);
        if (m > maxMag2) { maxMag2 = m; maxBin = k; }
    }
    std::printf("complex forward: max bin %d mag2x=%.2f abs=%.2f\n", maxBin, maxMag2, std::abs(out[static_cast<size_t>(maxBin)]));

    std::vector<float> realBuf(N * 2, 0.0f);
    for (int k = 0; k < N; ++k)
        realBuf[static_cast<size_t>(k)] = fifo[static_cast<size_t>(k)];
    fft.performRealOnlyForwardTransform(realBuf.data(), true);
    float maxRealMag = 0.0f;
    int maxRealBin = 0;
    for (int k = 0; k < N / 2 + 1; ++k)
    {
        const float re = realBuf[static_cast<size_t>(k * 2)];
        const float im = realBuf[static_cast<size_t>(k * 2 + 1)];
        const float m = 2.0f * std::hypot(re, im);
        if (m > maxRealMag) { maxRealMag = m; maxRealBin = k; }
    }
    std::printf("real forward: max bin %d mag2x=%.2f\n", maxRealBin, maxRealMag);
    return 0;
}
