#include "PitchShifter.h"

PitchShifter::PitchShifter() = default;

PitchShifter::PitchShifter(PitchShifter&& other) noexcept
    : fft(std::move(other.fft)),
      fftInput(std::move(other.fftInput)),
      fftOutput(std::move(other.fftOutput)),
      inputFifo(std::move(other.inputFifo)),
      outputFifo(std::move(other.outputFifo)),
      outputAccum(std::move(other.outputAccum)),
      window(std::move(other.window)),
      lastPhase(std::move(other.lastPhase)),
      sumPhase(std::move(other.sumPhase)),
      analysisMagnitudes(std::move(other.analysisMagnitudes)),
      analysisFrequencies(std::move(other.analysisFrequencies)),
      synthesisMagnitudes(std::move(other.synthesisMagnitudes)),
      synthesisFrequencies(std::move(other.synthesisFrequencies)),
      synthMaxMag(std::move(other.synthMaxMag)),
      rover(other.rover),
      fifoLatency(other.fifoLatency),
      warmupSamplesRemaining(other.warmupSamplesRemaining),
      sampleRate(other.sampleRate),
      prepared(other.prepared),
      lastProcessedRatio(other.lastProcessedRatio)
{
    other.prepared = false;
}

PitchShifter& PitchShifter::operator=(PitchShifter&& other) noexcept
{
    if (this != &other)
    {
        fft = std::move(other.fft);
        fftInput = std::move(other.fftInput);
        fftOutput = std::move(other.fftOutput);
        inputFifo = std::move(other.inputFifo);
        outputFifo = std::move(other.outputFifo);
        outputAccum = std::move(other.outputAccum);
        window = std::move(other.window);
        lastPhase = std::move(other.lastPhase);
        sumPhase = std::move(other.sumPhase);
        analysisMagnitudes = std::move(other.analysisMagnitudes);
        analysisFrequencies = std::move(other.analysisFrequencies);
        synthesisMagnitudes = std::move(other.synthesisMagnitudes);
        synthesisFrequencies = std::move(other.synthesisFrequencies);
        synthMaxMag = std::move(other.synthMaxMag);
        rover = other.rover;
        fifoLatency = other.fifoLatency;
        warmupSamplesRemaining = other.warmupSamplesRemaining;
        sampleRate = other.sampleRate;
        prepared = other.prepared;
        lastProcessedRatio = other.lastProcessedRatio;
        other.prepared = false;
    }
    return *this;
}

void PitchShifter::prepare(double sr, int)
{
    sampleRate = sr;
    fft = std::make_unique<juce::dsp::FFT>(kFftOrder);

    fftInput.assign(kFftSize, {});
    fftOutput.assign(kFftSize, {});
    inputFifo.assign(kFftSize, 0.0f);
    outputFifo.assign(kStepSize, 0.0f);
    outputAccum.assign(kFftSize * 2, 0.0f);
    window.resize(kFftSize);

    const int bins = kFftSize / 2 + 1;
    lastPhase.assign(bins, 0.0f);
    sumPhase.assign(bins, 0.0f);
    analysisMagnitudes.assign(bins, 0.0f);
    analysisFrequencies.assign(bins, 0.0f);
    synthesisMagnitudes.assign(bins, 0.0f);
    synthesisFrequencies.assign(bins, 0.0f);
    synthMaxMag.assign(bins, 0.0f);

    for (int i = 0; i < kFftSize; ++i)
        window[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(i) / static_cast<float>(kFftSize));

    fifoLatency = kFftSize - kStepSize;
    rover = fifoLatency;
    warmupSamplesRemaining = fifoLatency;
    prepared = true;
}

void PitchShifter::reset()
{
    std::fill(inputFifo.begin(), inputFifo.end(), 0.0f);
    std::fill(outputFifo.begin(), outputFifo.end(), 0.0f);
    std::fill(outputAccum.begin(), outputAccum.end(), 0.0f);
    std::fill(lastPhase.begin(), lastPhase.end(), 0.0f);
    std::fill(sumPhase.begin(), sumPhase.end(), 0.0f);
    std::fill(synthMaxMag.begin(), synthMaxMag.end(), 0.0f);
    rover = fifoLatency;
    warmupSamplesRemaining = fifoLatency;
    lastProcessedRatio = 1.0f;
}

void PitchShifter::resetPhaseAccumulators()
{
    std::fill(lastPhase.begin(), lastPhase.end(), 0.0f);
    std::fill(sumPhase.begin(), sumPhase.end(), 0.0f);
}

void PitchShifter::process(const float* input, float* output, int numSamples, float pitchRatio)
{
    constexpr float kUnityRatio = 0.001f;
    if (std::abs(pitchRatio - 1.0f) < kUnityRatio)
    {
        std::copy(input, input + numSamples, output);
        return;
    }

    if (!prepared || fft == nullptr)
    {
        std::copy(input, input + numSamples, output);
        return;
    }

    const float clampedRatio = std::clamp(pitchRatio, 0.50f, 2.0f);

    constexpr float kUnityThresh = 0.0006f;
    const bool nearUnity = std::abs(clampedRatio - 1.0f) < kUnityThresh;
    const bool wasNearUnity = std::abs(lastProcessedRatio - 1.0f) < kUnityThresh;
    if (nearUnity != wasNearUnity)
        resetPhaseAccumulators();
    lastProcessedRatio = clampedRatio;

    for (int i = 0; i < numSamples; ++i)
    {
        inputFifo[static_cast<size_t>(rover)] = input[i];

        if (warmupSamplesRemaining > 0)
        {
            output[i] = input[i];
            --warmupSamplesRemaining;
        }
        else
        {
            output[i] = outputFifo[static_cast<size_t>(rover - fifoLatency)];
        }

        if (++rover >= kFftSize)
        {
            rover = fifoLatency;
            processFrame(clampedRatio);

            for (int k = 0; k < fifoLatency; ++k)
                inputFifo[static_cast<size_t>(k)] = inputFifo[static_cast<size_t>(k + kStepSize)];
        }
    }

    constexpr float kMakeupGain = 2.2f;
    for (int i = 0; i < numSamples; ++i)
        output[i] *= kMakeupGain;
}

void PitchShifter::processFrame(float pitchRatio)
{
    const float freqPerBin = static_cast<float>(sampleRate) / static_cast<float>(kFftSize);
    const float expectedPhase = 2.0f * kPi * static_cast<float>(kStepSize) / static_cast<float>(kFftSize);
    const int bins = kFftSize / 2 + 1;

    for (int k = 0; k < kFftSize; ++k)
        fftInput[static_cast<size_t>(k)] = { inputFifo[static_cast<size_t>(k)] * window[static_cast<size_t>(k)], 0.0f };

    fft->perform(fftInput.data(), fftOutput.data(), false);

    std::fill(analysisMagnitudes.begin(), analysisMagnitudes.end(), 0.0f);
    std::fill(analysisFrequencies.begin(), analysisFrequencies.end(), 0.0f);
    std::fill(synthesisMagnitudes.begin(), synthesisMagnitudes.end(), 0.0f);
    std::fill(synthesisFrequencies.begin(), synthesisFrequencies.end(), 0.0f);
    std::fill(synthMaxMag.begin(), synthMaxMag.end(), 0.0f);

    for (int k = 0; k < bins; ++k)
    {
        const auto c = fftOutput[static_cast<size_t>(k)];
        const float magnitude = 2.0f * std::abs(c);
        const float phase = std::atan2(c.imag(), c.real());

        float phaseDelta = phase - lastPhase[static_cast<size_t>(k)];
        lastPhase[static_cast<size_t>(k)] = phase;

        phaseDelta -= static_cast<float>(k) * expectedPhase;
        int quadrant = static_cast<int>(phaseDelta / kPi);
        if (quadrant >= 0)
            quadrant += quadrant & 1;
        else
            quadrant -= quadrant & 1;
        phaseDelta -= kPi * static_cast<float>(quadrant);

        const float trueFrequency = (static_cast<float>(k) + phaseDelta * static_cast<float>(kOversampling) / (2.0f * kPi)) * freqPerBin;
        analysisMagnitudes[static_cast<size_t>(k)] = magnitude;
        analysisFrequencies[static_cast<size_t>(k)] = trueFrequency;
    }

    for (int k = 0; k < bins; ++k)
    {
        const int shiftedBin = static_cast<int>(static_cast<float>(k) * pitchRatio + 0.5f);
        if (shiftedBin < bins)
        {
            const float mag = analysisMagnitudes[static_cast<size_t>(k)];
            synthesisMagnitudes[static_cast<size_t>(shiftedBin)] += mag;
            if (mag > synthMaxMag[static_cast<size_t>(shiftedBin)])
            {
                synthMaxMag[static_cast<size_t>(shiftedBin)] = mag;
                synthesisFrequencies[static_cast<size_t>(shiftedBin)] = analysisFrequencies[static_cast<size_t>(k)] * pitchRatio;
            }
        }
    }

    std::fill(fftInput.begin(), fftInput.end(), std::complex<float> {});

    for (int k = 0; k < bins; ++k)
    {
        const float magnitude = synthesisMagnitudes[static_cast<size_t>(k)];
        float phaseDelta = synthesisFrequencies[static_cast<size_t>(k)] - static_cast<float>(k) * freqPerBin;
        phaseDelta /= freqPerBin;
        phaseDelta = 2.0f * kPi * phaseDelta / static_cast<float>(kOversampling);
        phaseDelta += static_cast<float>(k) * expectedPhase;

        sumPhase[static_cast<size_t>(k)] += phaseDelta;
        const float phase = sumPhase[static_cast<size_t>(k)];
        fftInput[static_cast<size_t>(k)] = { magnitude * std::cos(phase), magnitude * std::sin(phase) };
    }

    for (int k = 1; k < kFftSize / 2; ++k)
        fftInput[static_cast<size_t>(kFftSize - k)] = std::conj(fftInput[static_cast<size_t>(k)]);

    fft->perform(fftInput.data(), fftOutput.data(), true);

    const float scale = 1.0f / (static_cast<float>(kFftSize) * static_cast<float>(kOversampling));
    for (int k = 0; k < kFftSize; ++k)
    {
        outputAccum[static_cast<size_t>(k)] += 2.0f * window[static_cast<size_t>(k)] * fftOutput[static_cast<size_t>(k)].real() * scale;
    }

    for (int k = 0; k < kStepSize; ++k)
        outputFifo[static_cast<size_t>(k)] = outputAccum[static_cast<size_t>(k)];

    std::move(outputAccum.begin() + kStepSize, outputAccum.end(), outputAccum.begin());
    std::fill(outputAccum.end() - kStepSize, outputAccum.end(), 0.0f);
}
