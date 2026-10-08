#include "dsp/PitchDetector.h"
#include <cstdio>
#include <vector>

int main()
{
    bool pass = true;
    for (double rate : {44100., 48000., 96000.})
        for (float hz : {55.f, 130.f, 227.f, 340.f, 440.f, 880.f})
            for (int block : {1, 127, 512})
            {
                PitchDetector detector;
                detector.prepare(rate, block);
                detector.setFrequencyRange(55, 1000);
                const int total = static_cast<int>(rate * .15);
                std::vector<float> vocal(total);
                for (int i = 0; i < total; ++i)
                {
                    double phase = i * 6.283185307179586 * hz / rate;
                    vocal[i] =
                        .2f * std::sin(phase) + .08f * std::sin(2 * phase) + .04f * std::sin(3 * phase);
                }
                float detected = 0;
                for (int i = 0; i < total; i += block)
                    detected = detector.detectPitch(vocal.data() + i, std::min(block, total - i));
                const float error = detected > 0 ? std::abs(1200.f * std::log2(detected / hz)) : 10000.f;
                if (error > 3.f || detector.getConfidence() < .9f)
                {
                    std::printf("FAIL rate=%.0f block=%d input=%.1f detected=%.3f "
                                "error=%.2f cents\n",
                                rate, block, hz, detected, error);
                    pass = false;
                }
                detector.reset();
                std::vector<float> silence(4096);
                if (detector.detectPitch(silence.data(), silence.size()) != 0.f)
                    pass = false;
            }
    if (pass)
        std::puts("PASS harmonic vocal tracking: 3 sample rates, 6 pitches, 3 host "
                  "block sizes, silence reset");
    return pass ? 0 : 1;
}
