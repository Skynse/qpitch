#include "PitchShifter.h"
#include <juce_core/juce_core.h>

namespace
{
void* loadRubberBandSymbol(void* library, const char* name)
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
#else
    return dlsym(library, name);
#endif
}

void* openRubberBandLibraryHandle()
{
    juce::StringArray names;
#if defined(_WIN32)
    names.add("rubberband-3.dll");
    names.add("rubberband.dll");
    names.add("librubberband-2.dll");
    names.add("librubberband-3.dll");
#elif defined(__APPLE__)
    names.add("librubberband.3.dylib");
    names.add("librubberband.dylib");
#else
    names.add("librubberband.so.3");
    names.add("librubberband.so");
#endif

    juce::Array<juce::File> searchDirs;
    searchDirs.add(juce::File());

#if defined(_WIN32)
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(static_cast<HMODULE>(juce::Process::getCurrentModuleInstanceHandle()),
                           modulePath,
                           static_cast<DWORD>(juce::numElementsInArray(modulePath))) != 0)
        searchDirs.add(juce::File(juce::String(modulePath)).getParentDirectory());
#else
    Dl_info info {};
    if (dladdr(reinterpret_cast<void*>(&openRubberBandLibraryHandle), &info) != 0 && info.dli_fname != nullptr)
        searchDirs.add(juce::File(juce::String(info.dli_fname)).getParentDirectory());
#endif

    for (const auto& dir : searchDirs)
    {
        for (const auto& name : names)
        {
            const juce::String path = dir.getFullPathName().isEmpty()
                ? name
                : dir.getChildFile(name).getFullPathName();

#if defined(_WIN32)
            if (auto* handle = LoadLibraryW(juce::String(path).toWideCharPointer()))
                return handle;
#else
            if (auto* handle = dlopen(path.toRawUTF8(), RTLD_NOW | RTLD_LOCAL))
                return handle;
#endif
        }
    }

    return nullptr;
}
} // namespace

PitchShifter::PitchShifter() {}

PitchShifter::PitchShifter(PitchShifter&& other) noexcept
{
    *this = std::move(other);
}

PitchShifter& PitchShifter::operator=(PitchShifter&& other) noexcept
{
    if (this == &other)
        return *this;

    closeRubberBand();

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
    rbInputBlock = std::move(other.rbInputBlock);
    rbOutputBlock = std::move(other.rbOutputBlock);

    rbLibrary = other.rbLibrary;
    rbState = other.rbState;
    rbNew = other.rbNew;
    rbDelete = other.rbDelete;
    rbReset = other.rbReset;
    rbSetPitch = other.rbSetPitch;
    rbSetFormantOption = other.rbSetFormantOption;
    rbGetBlockSize = other.rbGetBlockSize;
    rbShift = other.rbShift;
    rbBlockSize = other.rbBlockSize;
    rbInputFill = other.rbInputFill;
    rbOutputRead = other.rbOutputRead;
    rbOutputAvailable = other.rbOutputAvailable;
    rubberBandReady = other.rubberBandReady;
    formantCorrected = other.formantCorrected;

    rover = other.rover;
    fifoLatency = other.fifoLatency;
    warmupSamplesRemaining = other.warmupSamplesRemaining;
    sampleRate = other.sampleRate;
    prepared = other.prepared;
    lastProcessedRatio = other.lastProcessedRatio;

    other.rbLibrary = nullptr;
    other.rbState = nullptr;
    other.rbNew = nullptr;
    other.rbDelete = nullptr;
    other.rbReset = nullptr;
    other.rbSetPitch = nullptr;
    other.rbSetFormantOption = nullptr;
    other.rbGetBlockSize = nullptr;
    other.rbShift = nullptr;
    other.rubberBandReady = false;
    other.prepared = false;

    return *this;
}

PitchShifter::~PitchShifter()
{
    closeRubberBand();
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
    rubberBandReady = prepareRubberBand();
    prepared = true;
}

void PitchShifter::reset()
{
    if (rubberBandReady && rbReset != nullptr && rbState != nullptr)
        rbReset(rbState);

    rbInputFill = 0;
    rbOutputRead = 0;
    rbOutputAvailable = 0;
    std::fill(rbInputBlock.begin(), rbInputBlock.end(), 0.0f);
    std::fill(rbOutputBlock.begin(), rbOutputBlock.end(), 0.0f);

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
    if (rubberBandReady)
    {
        processRubberBand(input, output, numSamples, pitchRatio);
        return;
    }

    if (!prepared || fft == nullptr)
    {
        std::copy(input, input + numSamples, output);
        return;
    }

    const float clampedRatio = std::clamp(pitchRatio, 0.50f, 2.0f);

    // Reset phase state when crossing unity — stale accumulators in the FFT
    // fallback cause destructive overlap-add cancellation (silence / comb filtering).
    constexpr float kUnityThresh = 0.0006f; // ~1 cent
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
}

void PitchShifter::closeRubberBand()
{
    if (rbState != nullptr && rbDelete != nullptr)
        rbDelete(rbState);
    rbState = nullptr;

    if (rbLibrary != nullptr)
    {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(rbLibrary));
#else
        dlclose(rbLibrary);
#endif
    }
    rbLibrary = nullptr;

    rbNew = nullptr;
    rbDelete = nullptr;
    rbReset = nullptr;
    rbSetPitch = nullptr;
    rbSetFormantOption = nullptr;
    rbGetBlockSize = nullptr;
    rbShift = nullptr;
    rubberBandReady = false;
}

bool PitchShifter::prepareRubberBand()
{
    closeRubberBand();

    rbLibrary = openRubberBandLibraryHandle();
    if (rbLibrary == nullptr)
        return false;

    rbNew = reinterpret_cast<RbNewFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_new"));
    rbDelete = reinterpret_cast<RbDeleteFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_delete"));
    rbReset = reinterpret_cast<RbResetFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_reset"));
    rbSetPitch = reinterpret_cast<RbSetPitchFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_set_pitch_scale"));
    rbSetFormantOption = reinterpret_cast<RbSetFormantOptionFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_set_formant_option"));
    rbGetBlockSize = reinterpret_cast<RbGetBlockSizeFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_get_block_size"));
    rbShift = reinterpret_cast<RbShiftFn>(loadRubberBandSymbol(rbLibrary, "rubberband_live_shift"));

    if (rbNew == nullptr || rbDelete == nullptr || rbReset == nullptr
        || rbSetPitch == nullptr || rbGetBlockSize == nullptr || rbShift == nullptr)
    {
        closeRubberBand();
        return false;
    }

    constexpr int options = 0x00000000; // OptionFormantShifted
    rbState = rbNew(static_cast<unsigned int>(sampleRate), 1, options);
    if (rbState == nullptr)
    {
        closeRubberBand();
        return false;
    }

    rbBlockSize = rbGetBlockSize(rbState);
    if (rbBlockSize == 0 || rbBlockSize > 8192)
    {
        closeRubberBand();
        return false;
    }

    rbInputBlock.assign(rbBlockSize, 0.0f);
    rbOutputBlock.assign(rbBlockSize, 0.0f);
    rbInputFill = 0;
    rbOutputRead = 0;
    rbOutputAvailable = 0;
    setFormantCorrected(formantCorrected);
    return true;
}

void PitchShifter::setFormantCorrected(bool shouldCorrect)
{
    formantCorrected = shouldCorrect;

    if (!rubberBandReady || rbSetFormantOption == nullptr || rbState == nullptr)
        return;

    constexpr int formantShifted = 0x00000000;
    constexpr int formantPreserved = 0x01000000;
    rbSetFormantOption(rbState, shouldCorrect ? formantPreserved : formantShifted);
}

void PitchShifter::processRubberBand(const float* input, float* output, int numSamples, float pitchRatio)
{
    const double clampedRatio = static_cast<double>(std::clamp(pitchRatio, 0.50f, 2.0f));
    rbSetPitch(rbState, clampedRatio);

    for (int i = 0; i < numSamples; ++i)
    {
        rbInputBlock[rbInputFill++] = input[i];

        if (rbOutputRead < rbOutputAvailable)
            output[i] = rbOutputBlock[rbOutputRead++];
        else
            output[i] = input[i];

        if (rbInputFill == rbBlockSize)
        {
            const float* inPtrs[] = { rbInputBlock.data() };
            float* outPtrs[] = { rbOutputBlock.data() };
            rbShift(rbState, inPtrs, outPtrs);
            rbInputFill = 0;
            rbOutputRead = 0;
            rbOutputAvailable = rbBlockSize;
        }
    }
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
            // Use frequency from the highest-magnitude source bin so the dominant
            // harmonic drives the synthesis phase rather than whichever bin mapped last.
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
