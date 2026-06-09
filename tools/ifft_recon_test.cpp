#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    constexpr int N = 1024;
    juce::dsp::FFT fft(10);
    std::vector<std::complex<float>> in(N), out(N), spec(N);

    for (int i = 0; i < N; ++i)
        in[static_cast<size_t>(i)] = { std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f), 0.0f };

    fft.perform(in.data(), out.data(), false);

    auto peakOf = [](const std::vector<std::complex<float>>& v) {
        float p = 0.0f;
        for (const auto& c : v) p = std::max(p, std::abs(c.real()));
        return p;
    };

  for (const char* mode : { "copy", "double", "mag2x" })
  {
        std::fill(spec.begin(), spec.end(), std::complex<float> {});
        if (std::string(mode) == "copy")
        {
            for (int k = 0; k < N; ++k) spec[static_cast<size_t>(k)] = out[static_cast<size_t>(k)];
        }
        else if (std::string(mode) == "double")
        {
            for (int k = 0; k < N; ++k) spec[static_cast<size_t>(k)] = out[static_cast<size_t>(k)] * 2.0f;
        }
        else
        {
            for (int k = 0; k < N / 2 + 1; ++k)
            {
                const float m = 2.0f * std::abs(out[static_cast<size_t>(k)]);
                const float p = std::atan2(out[static_cast<size_t>(k)].imag(), out[static_cast<size_t>(k)].real());
                spec[static_cast<size_t>(k)] = { m * std::cos(p), m * std::sin(p) };
            }
            for (int k = 1; k < N / 2; ++k)
                spec[static_cast<size_t>(N - k)] = std::conj(spec[static_cast<size_t>(k)]);
        }

        std::vector<std::complex<float>> recon(N);
        fft.perform(spec.data(), recon.data(), true);
        std::printf("%s ifft peak=%.6f\n", mode, peakOf(recon));
  }

  // real-only path
  std::vector<float> real(N * 2, 0.0f);
  for (int i = 0; i < N; ++i)
      real[static_cast<size_t>(i)] = in[static_cast<size_t>(i)].real();
  fft.performRealOnlyForwardTransform(real.data(), true);
  fft.performRealOnlyInverseTransform(real.data());
  float realPeak = 0.0f;
  for (int i = 0; i < N; ++i)
      realPeak = std::max(realPeak, std::abs(real[static_cast<size_t>(i)]));
  std::printf("real-only roundtrip peak=%.6f\n", realPeak);
  return 0;
}
