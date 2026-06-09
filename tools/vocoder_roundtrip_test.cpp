#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    constexpr int N = 1024;
    constexpr int OS = 4;
    constexpr int hop = N / OS;
    constexpr float sr = 44100.0f;
    const float scale = 1.0f / (static_cast<float>(N) * static_cast<float>(OS));

    juce::dsp::FFT fft(10);
    std::vector<std::complex<float>> fwdIn(N), fwdOut(N), syn(N);
    std::vector<float> window(N), fifo(N), outputAccum(N * 2, 0.0f), outputFifo(hop, 0.0f);

    for (int i = 0; i < N; ++i)
    {
        window[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(N));
        fifo[static_cast<size_t>(i)] = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / sr) * window[static_cast<size_t>(i)];
    }

    for (int k = 0; k < N; ++k)
        fwdIn[static_cast<size_t>(k)] = { fifo[static_cast<size_t>(k)], 0.0f };
    fft.perform(fwdIn.data(), fwdOut.data(), false);

    const int bins = N / 2 + 1;
    const float expectedPhase = 2.0f * 3.14159265f * static_cast<float>(hop) / static_cast<float>(N);
    const float freqPerBin = sr / static_cast<float>(N);

    std::fill(syn.begin(), syn.end(), std::complex<float> {});
    for (int k = 0; k < bins; ++k)
    {
        const auto c = fwdOut[static_cast<size_t>(k)];
        const float magnitude = 2.0f * std::abs(c);
        const float phase = std::atan2(c.imag(), c.real());
        syn[static_cast<size_t>(k)] = { magnitude * std::cos(phase), magnitude * std::sin(phase) };
        (void) expectedPhase;
        (void) freqPerBin;
    }
    for (int k = 1; k < N / 2; ++k)
        syn[static_cast<size_t>(N - k)] = std::conj(syn[static_cast<size_t>(k)]);

    std::vector<std::complex<float>> ifftOut(N);
    fft.perform(syn.data(), ifftOut.data(), true);

    for (int k = 0; k < N; ++k)
        outputAccum[static_cast<size_t>(k)] += 2.0f * window[static_cast<size_t>(k)] * ifftOut[static_cast<size_t>(k)].real() * scale;

    float peak = 0.0f;
    for (int k = 0; k < hop; ++k)
    {
        outputFifo[static_cast<size_t>(k)] = outputAccum[static_cast<size_t>(k)];
        peak = std::max(peak, std::abs(outputFifo[static_cast<size_t>(k)]));
    }
    std::printf("single-frame direct spectrum IFFT peak=%.6f\n", peak);

    // 100 frames of OLA at ratio 1
    std::fill(outputAccum.begin(), outputAccum.end(), 0.0f);
    std::vector<float> lastPhase(bins, 0.0f), sumPhase(bins, 0.0f);
    for (int frame = 0; frame < 200; ++frame)
    {
        for (int k = 0; k < N; ++k)
            fwdIn[static_cast<size_t>(k)] = { fifo[static_cast<size_t>(k)], 0.0f };
        fft.perform(fwdIn.data(), fwdOut.data(), false);

        std::fill(syn.begin(), syn.end(), std::complex<float> {});
        for (int k = 0; k < bins; ++k)
        {
            const auto c = fwdOut[static_cast<size_t>(k)];
            const float magnitude = 2.0f * std::abs(c);
            float phase = std::atan2(c.imag(), c.real());
            float phaseDelta = phase - lastPhase[static_cast<size_t>(k)];
            lastPhase[static_cast<size_t>(k)] = phase;
            phaseDelta -= static_cast<float>(k) * expectedPhase;
            int quadrant = static_cast<int>(phaseDelta / 3.14159265f);
            if (quadrant >= 0) quadrant += quadrant & 1; else quadrant -= quadrant & 1;
            phaseDelta -= 3.14159265f * static_cast<float>(quadrant);
            float phaseDeltaSynth = phaseDelta + static_cast<float>(k) * expectedPhase;
            sumPhase[static_cast<size_t>(k)] += phaseDeltaSynth;
            const float p = sumPhase[static_cast<size_t>(k)];
            syn[static_cast<size_t>(k)] = { magnitude * std::cos(p), magnitude * std::sin(p) };
        }
        for (int k = 1; k < N / 2; ++k)
            syn[static_cast<size_t>(N - k)] = std::conj(syn[static_cast<size_t>(k)]);

        fft.perform(syn.data(), ifftOut.data(), true);
        for (int k = 0; k < N; ++k)
            outputAccum[static_cast<size_t>(k)] += 2.0f * window[static_cast<size_t>(k)] * ifftOut[static_cast<size_t>(k)].real() * scale;

        std::move(outputAccum.begin() + hop, outputAccum.end(), outputAccum.begin());
        std::fill(outputAccum.end() - hop, outputAccum.end(), 0.0f);
    }
    float olaPeak = 0.0f;
    for (int k = 0; k < hop; ++k)
        olaPeak = std::max(olaPeak, std::abs(outputAccum[static_cast<size_t>(k)]));
    std::printf("200-frame PV OLA peak=%.6f\n", olaPeak);
    return 0;
}
