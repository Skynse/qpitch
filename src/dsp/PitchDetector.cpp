#include "PitchDetector.h"

PitchDetector::PitchDetector() {}

void PitchDetector::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr;
    this->maxBlockSize = maxBlockSize;
    setFrequencyRange(40.0f, 2000.0f);
}

void PitchDetector::setFrequencyRange(float minHz, float maxHz)
{
    maxHz = std::max(maxHz, minHz + 10.0f);
    minPeriod = static_cast<int>(sampleRate / maxHz);
    if (minPeriod < 4) minPeriod = 4;
    maxPeriod = static_cast<int>(sampleRate / minHz);
    if (maxPeriod > sampleRate / 2) maxPeriod = static_cast<int>(sampleRate / 2);
    int neededSize = maxBlockSize + maxPeriod + 4;
    workBuffer.resize(neededSize, 0.0f);
    diffBuffer.resize(maxPeriod + 1, 0.0f);
    normDiffBuffer.resize(maxPeriod + 1, 1.0f);
    historyFill = 0;
}

void PitchDetector::reset()
{
    std::fill(workBuffer.begin(), workBuffer.end(), 0.0f);
    std::fill(diffBuffer.begin(), diffBuffer.end(), 0.0f);
    std::fill(normDiffBuffer.begin(), normDiffBuffer.end(), 1.0f);
    historyFill = 0;
    confidence = 0.0f;
}

float PitchDetector::detectPitch(const float* buffer, int numSamples)
{
    if (workBuffer.empty() || numSamples <= 0)
        return 0.0f;

    const int historySize = static_cast<int>(workBuffer.size());
    const int samplesToCopy = std::min(numSamples, historySize);

    if (samplesToCopy < historySize)
    {
        std::move(workBuffer.begin() + samplesToCopy, workBuffer.end(), workBuffer.begin());
        std::copy(buffer + numSamples - samplesToCopy, buffer + numSamples, workBuffer.end() - samplesToCopy);
    }
    else
    {
        std::copy(buffer + numSamples - historySize, buffer + numSamples, workBuffer.begin());
    }

    historyFill = std::min(historySize, historyFill + samplesToCopy);

    if (historyFill < minPeriod * 2)
        return 0.0f;

    const float* analysis = workBuffer.data() + historySize - historyFill;
    int tau = yinPeriod(analysis, historyFill);
    if (tau < minPeriod)
    {
        confidence = 0.0f;
        return 0.0f;
    }
    return static_cast<float>(sampleRate / tau);
}

int PitchDetector::yinPeriod(const float* buffer, int numSamples)
{
    int maxTau = std::min(maxPeriod, numSamples / 2);
    if (maxTau <= minPeriod * 2)
        return -1;

    int N = numSamples - maxTau;
    if (N < maxTau / 2)
        return -1;

    std::fill(diffBuffer.begin(), diffBuffer.begin() + maxTau + 1, 0.0f);

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < N; ++j)
        {
            float d = buffer[j] - buffer[j + tau];
            sum += static_cast<double>(d) * d;
        }
        diffBuffer[tau] = static_cast<float>(sum);
    }

    std::fill(normDiffBuffer.begin(), normDiffBuffer.begin() + maxTau + 1, 1.0f);
    double runningSum = 0.0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        runningSum += static_cast<double>(diffBuffer[tau]);
        if (runningSum > 0.0)
            normDiffBuffer[tau] = diffBuffer[tau] * static_cast<float>(tau) / static_cast<float>(runningSum);
    }

    int bestTau = -1;
    float bestVal = 1.0f;
    bool foundBelowThreshold = false;

    for (int tau = minPeriod; tau <= maxTau; ++tau)
    {
        if (normDiffBuffer[tau] < bestVal)
        {
            bestVal = normDiffBuffer[tau];
            bestTau = tau;
        }

        if (!foundBelowThreshold && normDiffBuffer[tau] < threshold)
        {
            if (tau + 1 <= maxTau && normDiffBuffer[tau + 1] > normDiffBuffer[tau]
                && tau - 1 >= minPeriod && normDiffBuffer[tau - 1] > normDiffBuffer[tau])
            {
                bestTau = tau;
                bestVal = normDiffBuffer[tau];
                foundBelowThreshold = true;
            }
        }
    }

    if (bestTau < minPeriod)
    {
        confidence = 0.0f;
        return -1;
    }

    for (int divisor = 2; divisor <= 4; ++divisor)
    {
        const int candidate = bestTau / divisor;
        if (candidate >= minPeriod && candidate <= maxTau)
        {
            const float candidateVal = normDiffBuffer[static_cast<size_t>(candidate)];
            if (candidateVal < 0.32f && candidateVal <= bestVal * 1.85f)
            {
                bestTau = candidate;
                bestVal = candidateVal;
                break;
            }
        }
    }

    confidence = 1.0f - bestVal;

    float offset = 0.0f;
    if (bestTau > 0 && bestTau < maxTau)
        offset = parabolicInterpolation(normDiffBuffer.data(), bestTau, maxTau + 1);

    return bestTau + static_cast<int>(std::round(offset));
}

float PitchDetector::parabolicInterpolation(const float* diff, int tau, int len) const
{
    if (tau <= 0 || tau >= len - 1)
        return 0.0f;

    float a = diff[tau - 1];
    float b = diff[tau];
    float c = diff[tau + 1];

    float denom = 2.0f * (2.0f * b - a - c);
    if (std::abs(denom) < 1e-12f)
        return 0.0f;

    return (a - c) / denom;
}
