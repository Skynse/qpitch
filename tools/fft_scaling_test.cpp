#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    constexpr int N = 1024;
    juce::dsp::FFT fft(10);
    std::vector<std::complex<float>> in(N), out(N);

    for (int i = 0; i < N; ++i)
        in[static_cast<size_t>(i)] = { std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f), 0.0f };

    fft.perform(in.data(), out.data(), false);
    float maxMag = 0.0f;
    int maxBin = 0;
    for (int k = 0; k < N / 2 + 1; ++k)
    {
        const float m = std::abs(out[static_cast<size_t>(k)]);
        if (m > maxMag) { maxMag = m; maxBin = k; }
    }
    std::printf("forward max bin %d mag %.4f\n", maxBin, maxMag);

    fft.perform(out.data(), in.data(), true);
    float peak = 0.0f;
    for (int i = 0; i < N; ++i)
        peak = std::max(peak, std::abs(in[static_cast<size_t>(i)].real()));
    std::printf("roundtrip peak %.6f (expect ~1)\n", peak);
    return 0;
}
