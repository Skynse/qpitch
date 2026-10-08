#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

class PitchDetector
{
  public:
    PitchDetector();
    ~PitchDetector() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void setFrequencyRange(float minHz, float maxHz);
    float detectPitch(const float* buffer, int numSamples);
    float getConfidence() const { return confidence; }
    bool isPitchValid() const { return confidence > 0.8f; }
    void reset();

  private:
    float yinPeriod(const float* buffer, int numSamples);
    float parabolicInterpolation(const float* diff, int tau, int len) const;

    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    int minPeriod = 44;
    int maxPeriod = 882;
    float confidence = 0.0f;
    static constexpr float threshold = 0.15f;
    std::vector<float> workBuffer;
    std::vector<float> diffBuffer;
    std::vector<float> normDiffBuffer;
    int historyFill = 0;
    int samplesSinceAnalysis = 0;
    float lastPitch = 0.0f;
};
