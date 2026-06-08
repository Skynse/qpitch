#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>
#include <memory>

#if defined(_WIN32)
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

class PitchShifter {
public:
    PitchShifter();
    PitchShifter(PitchShifter&& other) noexcept;
    PitchShifter& operator=(PitchShifter&& other) noexcept;
    PitchShifter(const PitchShifter&) = delete;
    PitchShifter& operator=(const PitchShifter&) = delete;
    ~PitchShifter();

    void prepare(double sampleRate, int maxBlockSize);
    void process(const float* input, float* output, int numSamples, float pitchRatio);
    void reset();
    bool isUsingRubberBand() const { return rubberBandReady; }
    void setFormantCorrected(bool shouldCorrect);

private:
    struct RubberBandLiveState_;
    using RubberBandLiveState = RubberBandLiveState_*;
    using RbNewFn = RubberBandLiveState (*)(unsigned int, unsigned int, int);
    using RbDeleteFn = void (*)(RubberBandLiveState);
    using RbResetFn = void (*)(RubberBandLiveState);
    using RbSetPitchFn = void (*)(RubberBandLiveState, double);
    using RbSetFormantOptionFn = void (*)(RubberBandLiveState, int);
    using RbGetBlockSizeFn = unsigned int (*)(RubberBandLiveState);
    using RbShiftFn = void (*)(RubberBandLiveState, const float* const*, float* const*);

    void closeRubberBand();
    bool prepareRubberBand();
    void processRubberBand(const float* input, float* output, int numSamples, float pitchRatio);
    void processFrame(float pitchRatio);
    void resetPhaseAccumulators();

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
    std::vector<float> rbInputBlock;
    std::vector<float> rbOutputBlock;

    void* rbLibrary = nullptr;
    RubberBandLiveState rbState = nullptr;
    RbNewFn rbNew = nullptr;
    RbDeleteFn rbDelete = nullptr;
    RbResetFn rbReset = nullptr;
    RbSetPitchFn rbSetPitch = nullptr;
    RbSetFormantOptionFn rbSetFormantOption = nullptr;
    RbGetBlockSizeFn rbGetBlockSize = nullptr;
    RbShiftFn rbShift = nullptr;
    unsigned int rbBlockSize = 0;
    unsigned int rbInputFill = 0;
    unsigned int rbOutputRead = 0;
    unsigned int rbOutputAvailable = 0;
    bool rubberBandReady = false;
    bool formantCorrected = false;

    int rover = 0;
    int fifoLatency = kFftSize - kStepSize;
    int warmupSamplesRemaining = 0;
    double sampleRate = 44100.0;
    bool prepared = false;
    float lastProcessedRatio = 1.0f;
};
