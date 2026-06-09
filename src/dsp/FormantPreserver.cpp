#include "FormantPreserver.h"

FormantPreserver::FormantPreserver() {}

void FormantPreserver::prepare(double sr, int, int)
{
    sampleRate = sr;
    const float cutoffHz = 1200.0f;
    lowCoeff = 1.0f - std::exp(-2.0f * 3.14159265358979323846f * cutoffHz
                                / static_cast<float>(sampleRate));
    envCoeff = 1.0f - std::exp(-1.0f / std::max(1.0f, 0.012f * static_cast<float>(sampleRate)));
    reset();
    prepared = true;
}

void FormantPreserver::reset()
{
    lowInState = 0.0f;
    lowShiftState = 0.0f;
    lowInEnv = 0.0f;
    lowShiftEnv = 0.0f;
    highInEnv = 0.0f;
    highShiftEnv = 0.0f;
}

void FormantPreserver::process(const float* input, const float* pitchShifted, float* output,
                               int numSamples, bool formantOn, float formantAmount)
{
    if (!prepared || !formantOn || formantAmount <= 0.0f)
    {
        std::copy(pitchShifted, pitchShifted + numSamples, output);
        return;
    }

    const float amount = std::clamp(formantAmount, 0.0f, 1.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float in = input[i];
        const float shifted = pitchShifted[i];

        lowInState += lowCoeff * (in - lowInState);
        lowShiftState += lowCoeff * (shifted - lowShiftState);

        const float highIn = in - lowInState;
        const float highShift = shifted - lowShiftState;

        lowInEnv += envCoeff * (std::abs(lowInState) - lowInEnv);
        lowShiftEnv += envCoeff * (std::abs(lowShiftState) - lowShiftEnv);
        highInEnv += envCoeff * (std::abs(highIn) - highInEnv);
        highShiftEnv += envCoeff * (std::abs(highShift) - highShiftEnv);

        const float bodyGain = std::clamp(lowInEnv / (lowShiftEnv + 1.0e-4f), 0.60f, 1.65f);
        const float airRatio = highInEnv / (highShiftEnv + 1.0e-4f);
        const float airGain = std::clamp(std::pow(airRatio, 0.50f), 0.85f, 1.40f);
        const float compensated = lowShiftState * (1.0f + amount * (bodyGain - 1.0f))
                                  + highShift * (1.0f + amount * 0.55f * (airGain - 1.0f));

        output[i] = std::clamp(compensated, -2.0f, 2.0f);
    }
}
