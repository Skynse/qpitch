#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>
#include <memory>

class PitchShifter {
public:
    PitchShifter();
    PitchShifter(PitchShifter&& other) noexcept;
    PitchShifter& operator=(PitchShifter&& other) noexcept;
    PitchShifter(const PitchShifter&) = delete;
    PitchShifter& operator=(const PitchShifter&) = delete;
    ~PitchShifter() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void process(const float* input, float* output, int numSamples, float pitchRatio);
    void reset();

private:
    void processFrame(float pitchRatio);

    static constexpr int kFftOrder = 10;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kOversampling = 4;
    static constexpr int kStepSize = kFftSize / kOversampling;
    static constexpr float kPi = 3.14159265358979323846f;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<std::complex<float>> fftInput;
    std::vector<std::complex<float>> fftOutput;
    std::vector<float> inputFifo;
    std::vector<float> outputFifo;
    std::vector<float> outputAccum;
    std::vector<float> window;
    std::vector<float> lastPhase;
    std::vector<float> sumPhase;
    std::vector<float> analysisMagnitudes;
    std::vector<float> analysisFrequencies;
    std::vector<float> synthesisMagnitudes;
    std::vector<float> synthesisFrequencies;
    std::vector<float> synthMaxMag;

    int rover = 0;
    int fifoLatency = kFftSize - kStepSize;
    double sampleRate = 44100.0;
    bool prepared = false;
};
