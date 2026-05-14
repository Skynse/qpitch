#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class FormantPreserver {
public:
    FormantPreserver();
    ~FormantPreserver() = default;

    void prepare(double sampleRate, int maxBlockSize, int lpcOrder = 12);
    void process(const float* input, const float* pitchShifted, float* output,
                 int numSamples, bool formantOn, float formantAmount);
    void reset();

private:
    double sampleRate = 44100.0;
    float lowCoeff = 0.0f;
    float envCoeff = 0.0f;
    float lowInState = 0.0f;
    float lowShiftState = 0.0f;
    float lowInEnv = 0.0f;
    float lowShiftEnv = 0.0f;
    float highInEnv = 0.0f;
    float highShiftEnv = 0.0f;
    bool prepared = false;
};
