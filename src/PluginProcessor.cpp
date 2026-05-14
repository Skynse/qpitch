#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <limits>

static const juce::String paramRetuneSpeed = "retune_speed";
static const juce::String paramNoteTransition = "note_transition";
static const juce::String paramCorrectionOn = "correction_on";
static const juce::String paramCorrectionAmount = "correction_amount";
static const juce::String paramToleranceCents = "tolerance_cents";
static const juce::String paramToleranceTime = "tolerance_time";
static const juce::String paramSnappiness = "snappiness";
static const juce::String paramTPain = "tpain";
static const juce::String paramReferenceFrequency = "reference_frequency";
static const juce::String paramKey = "key";
static const juce::String paramScale = "scale";
static const juce::String paramRange = "range";
static const juce::String paramFormantOn = "formant_on";
static const juce::String paramHumanize = "humanize";
static const juce::String paramOutputGain = "output_gain";

QPitchAudioProcessor::QPitchAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      vts(*this, nullptr, juce::Identifier("Parameters"), createParameterLayout())
{
    retuneSpeedParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramRetuneSpeed));
    noteTransitionParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramNoteTransition));
    correctionOnParam = static_cast<juce::AudioParameterBool*>(vts.getParameter(paramCorrectionOn));
    correctionAmountParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramCorrectionAmount));
    toleranceCentsParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramToleranceCents));
    toleranceTimeParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramToleranceTime));
    snappinessParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramSnappiness));
    tPainParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramTPain));
    referenceFrequencyParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramReferenceFrequency));
    keyParam = static_cast<juce::AudioParameterChoice*>(vts.getParameter(paramKey));
    scaleParam = static_cast<juce::AudioParameterChoice*>(vts.getParameter(paramScale));
    rangeParam = static_cast<juce::AudioParameterChoice*>(vts.getParameter(paramRange));
    formantOnParam = static_cast<juce::AudioParameterBool*>(vts.getParameter(paramFormantOn));
    humanizeParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramHumanize));
    outputGainParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramOutputGain));

    vts.addParameterListener(paramRetuneSpeed, this);
    vts.addParameterListener(paramReferenceFrequency, this);
    vts.addParameterListener(paramKey, this);
    vts.addParameterListener(paramScale, this);
    vts.addParameterListener(paramRange, this);

    currentKey = keyParam->getIndex();
    currentScale = scaleParam->getIndex();
    currentRange = rangeParam->getIndex();
    updateScaleMask();
    resetCustomNotesToScale();
}

QPitchAudioProcessor::~QPitchAudioProcessor()
{
    vts.removeParameterListener(paramRetuneSpeed, this);
    vts.removeParameterListener(paramReferenceFrequency, this);
    vts.removeParameterListener(paramKey, this);
    vts.removeParameterListener(paramScale, this);
    vts.removeParameterListener(paramRange, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout QPitchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramRetuneSpeed, "Retune Speed",
        juce::NormalisableRange<float>(0.0f, 800.0f, 0.1f), 15.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramNoteTransition, "Note Transition",
        juce::NormalisableRange<float>(0.0f, 800.0f, 0.1f), 120.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        paramCorrectionOn, "Correction On", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramCorrectionAmount, "Correction",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 100.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " %"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramToleranceCents, "Tolerance Cents",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ct"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramToleranceTime, "Tolerance Time",
        juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 0) + " ms"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramSnappiness, "Snappiness",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 0) + " %"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramTPain, "T-Pain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 0) + " %"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramReferenceFrequency, "Reference Frequency",
        juce::NormalisableRange<float>(415.0f, 466.0f, 0.01f), 440.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + " Hz"; })));

    juce::StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add(ScaleQuantizer::getKeyName(i));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        paramKey, "Key", keys, 0));

    juce::StringArray scales;
    for (int i = 0; i < ScaleQuantizer::numScales(); ++i)
        scales.add(ScaleQuantizer::getScaleName(i));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        paramScale, "Scale", scales, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        paramRange, "Range",
        juce::StringArray { "Bass", "Baritone", "Tenor", "Alto", "Mezzo Soprano", "Soprano", "Generic" }, 6));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        paramFormantOn, "Formant On", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramHumanize, "Humanize",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ct"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramOutputGain, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));

    return { params.begin(), params.end() };
}

void QPitchAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == paramRetuneSpeed)
    {
        float speedMs = newValue;
        if (speedMs <= 0.0f)
            pitchCoefficient = 0.0f;
        else
        {
            float speedSamples = speedMs * static_cast<float>(currentSampleRate) / 1000.0f;
            pitchCoefficient = std::exp(-1.0f / speedSamples);
        }
    }
    else if (parameterID == paramKey)
    {
        currentKey = static_cast<int>(newValue);
        lockedTargetMidi = -1;
        updateScaleMask();
        resetCustomNotesToScale();
    }
    else if (parameterID == paramScale)
    {
        currentScale = static_cast<int>(newValue);
        lockedTargetMidi = -1;
        updateScaleMask();
        resetCustomNotesToScale();
    }
    else if (parameterID == paramRange)
    {
        currentRange = static_cast<int>(newValue);
        updatePitchRange();
        lockedTargetMidi = -1;
        smoothedInputMidi = -1.0f;
        smoothedTargetMidi = -1.0f;
    }
    else if (parameterID == paramReferenceFrequency)
    {
        scaleQuantizer.setReferenceFrequency(newValue);
        lockedTargetMidi = -1;
        pendingTargetMidi = -1;
        smoothedInputMidi = -1.0f;
        smoothedTargetMidi = -1.0f;
    }
}

void QPitchAudioProcessor::updateScaleMask()
{
    if (currentScale >= 0 && currentScale < ScaleQuantizer::numScales())
    {
        const auto& masks = ScaleQuantizer::getScaleMasks();
        currentScaleMask = masks[currentScale];
    }
}

bool QPitchAudioProcessor::isCustomNoteEnabled(int noteClass) const
{
    noteClass = (noteClass % 12 + 12) % 12;
    return customNoteMask[static_cast<size_t>(noteClass)].load();
}

void QPitchAudioProcessor::setCustomNoteEnabled(int noteClass, bool enabled)
{
    noteClass = (noteClass % 12 + 12) % 12;
    customNoteMask[static_cast<size_t>(noteClass)].store(enabled);
    lockedTargetMidi = -1;
    pendingTargetMidi = -1;
    pendingTargetSamples = 0;
    smoothedTargetMidi = -1.0f;
}

void QPitchAudioProcessor::resetCustomNotesToScale()
{
    for (int note = 0; note < 12; ++note)
        customNoteMask[static_cast<size_t>(note)].store(currentScaleMask[static_cast<size_t>((note - currentKey + 12) % 12)]);
    lockedTargetMidi = -1;
    smoothedTargetMidi = -1.0f;
}

void QPitchAudioProcessor::updatePitchRange()
{
    struct RangeHz { float minHz; float maxHz; };
    static constexpr RangeHz ranges[] = {
        { 55.0f, 220.0f },  // Bass
        { 70.0f, 280.0f },  // Baritone
        { 90.0f, 360.0f },  // Tenor
        { 130.0f, 520.0f }, // Alto
        { 165.0f, 700.0f }, // Mezzo Soprano
        { 220.0f, 1000.0f },// Soprano
        { 55.0f, 1000.0f }  // Generic
    };

    const int index = juce::jlimit(0, 6, currentRange);
    pitchDetector.setFrequencyRange(ranges[index].minHz, ranges[index].maxHz);
}

int QPitchAudioProcessor::findNearestScaleMidi(float midiNote) const
{
    int bestMidi = static_cast<int>(std::round(midiNote));
    float bestDistance = std::numeric_limits<float>::max();

    const int center = static_cast<int>(std::round(midiNote));
    for (int candidate = center - 12; candidate <= center + 12; ++candidate)
    {
        const int noteClass = (candidate % 12 + 12) % 12;
        if (customNoteMask[static_cast<size_t>(noteClass)].load())
        {
            const float distance = std::abs(static_cast<float>(candidate) - midiNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestMidi = candidate;
            }
        }
    }

    return bestMidi;
}

void QPitchAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    pitchDetector.prepare(sampleRate, samplesPerBlock);
    updatePitchRange();
    const int channels = std::max(1, getTotalNumInputChannels());
    pitchShifters.resize(static_cast<size_t>(channels));
    formantPreservers.resize(static_cast<size_t>(channels));
    for (int ch = 0; ch < channels; ++ch)
    {
        pitchShifters[static_cast<size_t>(ch)].prepare(sampleRate, samplesPerBlock);
        formantPreservers[static_cast<size_t>(ch)].prepare(sampleRate, samplesPerBlock);
    }
    dryBuffer.setSize(channels, samplesPerBlock, false, true, true);
    shiftedBuffer.setSize(channels, samplesPerBlock, false, true, true);
    formantBuffer.setSize(channels, samplesPerBlock, false, true, true);
    airDryLowpassState.assign(static_cast<size_t>(channels), 0.0f);
    airWetLowpassState.assign(static_cast<size_t>(channels), 0.0f);

    currentSmoothedPitch = 0.0f;
    currentPitchRatio = 1.0f;
    humanizePhase = 0.0f;
    currentWetMix = 0.0f;
    smoothedCorrectionCents = 0.0f;
    smoothedTargetMidi = -1.0f;
    smoothedInputMidi = -1.0f;
    lockedTargetMidi = -1;
    pitchHoldSamples = 0;
    pendingTargetMidi = -1;
    pendingTargetSamples = 0;
    scaleQuantizer.setReferenceFrequency(referenceFrequencyParam->get());

    float speedMs = retuneSpeedParam->get();
    if (speedMs <= 0.0f)
        pitchCoefficient = 0.0f;
    else
    {
        float speedSamples = speedMs * static_cast<float>(sampleRate) / 1000.0f;
        pitchCoefficient = std::exp(-1.0f / speedSamples);
    }
}

void QPitchAudioProcessor::releaseResources()
{
    pitchDetector.reset();
    for (auto& shifter : pitchShifters)
        shifter.reset();
    for (auto& preserver : formantPreservers)
        preserver.reset();
    std::fill(airDryLowpassState.begin(), airDryLowpassState.end(), 0.0f);
    std::fill(airWetLowpassState.begin(), airWetLowpassState.end(), 0.0f);
}

void QPitchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const float prevPitchRatio = currentPitchRatio;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    float outputGainDb = outputGainParam->get();

    if (bypass || !correctionOnParam->get() || correctionAmountParam->get() <= 0.0f)
    {
        if (outputGainDb != 0.0f)
            buffer.applyGain(juce::Decibels::decibelsToGain(outputGainDb));
        return;
    }

    float corrAmount = correctionAmountParam->get() / 100.0f;
    const float toleranceCents = toleranceCentsParam->get();
    const float toleranceTimeMs = toleranceTimeParam->get();
    const float snappiness = snappinessParam->get() / 100.0f;
    const float tPain = tPainParam->get() / 100.0f;
    scaleQuantizer.setReferenceFrequency(referenceFrequencyParam->get());
    bool formantOn = formantOnParam->get();
    float formantAmount = formantOn ? 1.0f : 0.0f;
    float humanizeCents = humanizeParam->get();

    if (shiftedBuffer.getNumSamples() < numSamples || shiftedBuffer.getNumChannels() < totalNumInputChannels)
    {
        dryBuffer.setSize(totalNumInputChannels, numSamples, false, true, true);
        shiftedBuffer.setSize(totalNumInputChannels, numSamples, false, true, true);
        formantBuffer.setSize(totalNumInputChannels, numSamples, false, true, true);
    }

    dryBuffer.makeCopyOf(buffer, true);

    float* channelData = buffer.getWritePointer(0);
    float detectedHz = pitchDetector.detectPitch(channelData, numSamples);
    const float confidence = pitchDetector.getConfidence();
    const bool pitchValid = detectedHz > 0.0f && confidence > 0.62f;
    const float speedMsCurrent = retuneSpeedParam != nullptr ? retuneSpeedParam->get() : 15.0f;
    const float transitionMsCurrent = noteTransitionParam != nullptr ? noteTransitionParam->get() : 120.0f;
    const float effectiveSpeedMs = std::max(0.0f, speedMsCurrent * (1.0f - snappiness * 0.85f) - tPain * 8.0f);
    const float effectiveTransitionMs = std::max(0.0f, transitionMsCurrent * (1.0f - snappiness * 0.90f) * (1.0f - tPain * 0.95f));
    const bool roboticSnap = effectiveSpeedMs <= 5.0f || effectiveTransitionMs <= 12.0f || tPain >= 0.75f;
    const int maxHoldSamples = static_cast<int>((roboticSnap ? 0.120 : 0.180) * currentSampleRate);

    if (pitchValid)
    {
        float inputMidi = scaleQuantizer.hzToMidi(detectedHz);
        if (smoothedInputMidi >= 0.0f)
        {
            while (inputMidi - smoothedInputMidi > 6.0f)
                inputMidi -= 12.0f;
            while (smoothedInputMidi - inputMidi > 6.0f)
                inputMidi += 12.0f;
        }

        if (smoothedInputMidi < 0.0f)
            smoothedInputMidi = inputMidi;

        const float inputFollowCoeff = std::exp(-static_cast<float>(numSamples)
                                                / std::max(1.0f, 0.045f * static_cast<float>(currentSampleRate)));
        smoothedInputMidi = smoothedInputMidi * inputFollowCoeff + inputMidi * (1.0f - inputFollowCoeff);

        const float speedMs = effectiveSpeedMs;
        const bool hardSnap = speedMs <= 10.0f;
        const int nearestMidi = findNearestScaleMidi(inputMidi);

        if (lockedTargetMidi < 0)
        {
            lockedTargetMidi = nearestMidi;
            smoothedTargetMidi = static_cast<float>(lockedTargetMidi);
        }
        else if (nearestMidi != lockedTargetMidi)
        {
            const float currentDistance = std::abs(inputMidi - static_cast<float>(lockedTargetMidi));
            const float newDistance = std::abs(inputMidi - static_cast<float>(nearestMidi));
            const float hysteresis = roboticSnap ? 0.14f : (hardSnap ? 0.32f : 0.35f + toleranceCents * 0.004f);
            const float forcedDistance = roboticSnap ? 0.58f : (hardSnap ? 1.00f : 1.20f);
            if (newDistance + hysteresis < currentDistance || currentDistance > forcedDistance)
            {
                const float effectiveToleranceTimeMs = toleranceTimeMs * (1.0f - snappiness * 0.75f) * (1.0f - tPain);
                const int toleranceSamples = static_cast<int>(effectiveToleranceTimeMs * 0.001f * static_cast<float>(currentSampleRate));
                if (toleranceSamples <= 0 || roboticSnap || currentDistance > forcedDistance)
                {
                    lockedTargetMidi = nearestMidi;
                    pendingTargetMidi = -1;
                    pendingTargetSamples = 0;
                }
                else
                {
                    if (pendingTargetMidi != nearestMidi)
                    {
                        pendingTargetMidi = nearestMidi;
                        pendingTargetSamples = 0;
                    }
                    pendingTargetSamples += numSamples;
                    if (pendingTargetSamples >= toleranceSamples)
                    {
                        lockedTargetMidi = pendingTargetMidi;
                        pendingTargetMidi = -1;
                        pendingTargetSamples = 0;
                    }
                }
            }
        }
        else
        {
            pendingTargetMidi = -1;
            pendingTargetSamples = 0;
        }

        if (smoothedTargetMidi < 0.0f)
            smoothedTargetMidi = static_cast<float>(lockedTargetMidi);

        const float transitionMs = effectiveTransitionMs;
        if (transitionMs <= 20.0f)
        {
            smoothedTargetMidi = static_cast<float>(lockedTargetMidi);
        }
        else
        {
            const float transitionCoeff = std::exp(-static_cast<float>(numSamples)
                                                   / std::max(1.0f, transitionMs * 0.001f * static_cast<float>(currentSampleRate)));
            smoothedTargetMidi = smoothedTargetMidi * transitionCoeff
                                 + static_cast<float>(lockedTargetMidi) * (1.0f - transitionCoeff);
        }

        humanizePhase += static_cast<float>(numSamples) / static_cast<float>(currentSampleRate) * 0.55f;
        if (humanizePhase >= 1.0f)
            humanizePhase -= 1.0f;

        const float humanizeOffset = humanizeCents > 0.0f && !roboticSnap
            ? std::sin(humanizePhase * 2.0f * juce::MathConstants<float>::pi) * humanizeCents * 0.20f * (1.0f - tPain)
            : 0.0f;

        const float correctionInputMidi = hardSnap ? inputMidi : inputMidi;
        float rawCorrectionCents = (smoothedTargetMidi - correctionInputMidi) * 100.0f;
        const float effectiveToleranceCents = toleranceCents * (1.0f - snappiness * 0.75f) * (1.0f - tPain);
        if (effectiveToleranceCents > 0.0f)
        {
            if (std::abs(rawCorrectionCents) <= effectiveToleranceCents)
                rawCorrectionCents = 0.0f;
            else
                rawCorrectionCents -= std::copysign(effectiveToleranceCents, rawCorrectionCents);
        }

        const float targetCorrectionCents =
            std::clamp(rawCorrectionCents * corrAmount + humanizeOffset,
                       -1200.0f, 1200.0f);

        if (hardSnap || roboticSnap || snappiness >= 0.85f || tPain >= 0.50f)
        {
            smoothedCorrectionCents = targetCorrectionCents;
        }
        else if (pitchCoefficient > 0.0f && pitchCoefficient < 1.0f)
        {
            const float localCoeff = speedMs <= 0.0f
                ? 0.0f
                : std::exp(-1.0f / std::max(1.0f, speedMs * static_cast<float>(currentSampleRate) / 1000.0f));
            const float blockCoeff = std::pow(localCoeff, static_cast<float>(numSamples));
            smoothedCorrectionCents = smoothedCorrectionCents * blockCoeff
                                      + targetCorrectionCents * (1.0f - blockCoeff);
        }
        else
        {
            smoothedCorrectionCents = targetCorrectionCents;
        }

        currentPitchRatio = std::clamp(std::pow(2.0f, smoothedCorrectionCents / 1200.0f), 0.5f, 2.0f);
        debugDetectedHz.store(detectedHz);
        debugTargetHz.store(scaleQuantizer.midiToHz(smoothedTargetMidi));
        debugCorrectionCents.store(smoothedCorrectionCents);
        pitchHoldSamples = maxHoldSamples;
    }
    else if (pitchHoldSamples > 0)
    {
        pitchHoldSamples = std::max(0, pitchHoldSamples - numSamples);
    }
    else
    {
        lockedTargetMidi = -1;
        smoothedTargetMidi = -1.0f;
        smoothedInputMidi = -1.0f;
        const float releaseCoeff = std::exp(-static_cast<float>(numSamples) / std::max(1.0f, 0.030f * static_cast<float>(currentSampleRate)));
        smoothedCorrectionCents *= releaseCoeff;
        currentPitchRatio = std::clamp(std::pow(2.0f, smoothedCorrectionCents / 1200.0f), 0.5f, 2.0f);
        debugCorrectionCents.store(smoothedCorrectionCents);
        if (std::abs(smoothedCorrectionCents) < 1.0f)
        {
            debugDetectedHz.store(0.0f);
            debugTargetHz.store(0.0f);
        }
    }

    const float targetWet = 1.0f;
    currentWetMix = targetWet;

    if (true)
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            float* chData = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            float* shifted = shiftedBuffer.getWritePointer(ch);
            float* formant = formantBuffer.getWritePointer(ch);
            pitchShifters[static_cast<size_t>(ch)].setFormantCorrected(formantOn);

            constexpr int kSubBlockSize = 64;
            for (int offset = 0; offset < numSamples; offset += kSubBlockSize)
            {
                const int subLen = std::min(kSubBlockSize, numSamples - offset);
                const float t = numSamples > 1
                    ? static_cast<float>(offset) / static_cast<float>(numSamples - 1)
                    : 1.0f;
                const float subRatio = roboticSnap ? currentPitchRatio
                                                   : prevPitchRatio + (currentPitchRatio - prevPitchRatio) * t;
                pitchShifters[static_cast<size_t>(ch)].process(
                    dry + offset, shifted + offset, subLen, subRatio);
            }

            const float* processed = shifted;
            if (formantOn && formantAmount > 0.0f && !pitchShifters[static_cast<size_t>(ch)].isUsingRubberBand())
            {
                formantPreservers[static_cast<size_t>(ch)].process(dry, shifted, formant,
                                                                    numSamples, formantOn, formantAmount);
                processed = formant;
            }

            float dryLp = airDryLowpassState[static_cast<size_t>(ch)];
            float wetLp = airWetLowpassState[static_cast<size_t>(ch)];
            constexpr float airCutoffHz = 5200.0f;
            constexpr float airPreserve = 0.78f;
            const float lpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * airCutoffHz
                                           / static_cast<float>(currentSampleRate));

            for (int i = 0; i < numSamples; ++i)
            {
                dryLp = dry[i] + lpCoeff * (dryLp - dry[i]);
                wetLp = processed[i] + lpCoeff * (wetLp - processed[i]);

                const float dryAir = dry[i] - dryLp;
                const float wetAir = processed[i] - wetLp;
                const float restored = wetLp + dryAir * airPreserve + wetAir * (1.0f - airPreserve);
                chData[i] = currentWetMix >= 0.995f ? restored : dry[i] + (restored - dry[i]) * currentWetMix;
            }

            airDryLowpassState[static_cast<size_t>(ch)] = dryLp;
            airWetLowpassState[static_cast<size_t>(ch)] = wetLp;
        }
    }

    if (outputGainDb != 0.0f)
    {
        float gain = juce::Decibels::decibelsToGain(outputGainDb);
        buffer.applyGain(gain);
    }
}

void QPitchAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = vts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void QPitchAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(vts.state.getType()))
        vts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* QPitchAudioProcessor::createEditor()
{
    return new QPitchAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QPitchAudioProcessor();
}
