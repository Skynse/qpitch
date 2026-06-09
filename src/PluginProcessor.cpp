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

static juce::String noteParamId(int pitchClass)
{
    return "note_" + juce::String(pitchClass);
}

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
    for (int note = 0; note < 12; ++note)
        customNoteParams[static_cast<size_t>(note)] =
            static_cast<juce::AudioParameterBool*>(vts.getParameter(noteParamId(note)));

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

    static const char* pitchClassNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (int note = 0; note < 12; ++note)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            noteParamId(note), juce::String("Note ") + pitchClassNames[note], true));
    }

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
        const int newKey = static_cast<int>(newValue);
        if (newKey == currentKey)
            return;

        currentKey = newKey;
        lockedTargetMidi = -1;
        updateScaleMask();
        if (!isRestoringState)
            resetCustomNotesToScale();
    }
    else if (parameterID == paramScale)
    {
        const int newScale = static_cast<int>(newValue);
        if (newScale == currentScale)
            return;

        currentScale = newScale;
        lockedTargetMidi = -1;
        updateScaleMask();
        if (!isRestoringState)
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
    if (auto* param = customNoteParams[static_cast<size_t>(noteClass)])
        return param->get();
    return false;
}

void QPitchAudioProcessor::setCustomNoteEnabled(int noteClass, bool enabled)
{
    noteClass = (noteClass % 12 + 12) % 12;
    if (auto* param = customNoteParams[static_cast<size_t>(noteClass)])
        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
    lockedTargetMidi = -1;
    pendingTargetMidi = -1;
    pendingTargetSamples = 0;
    smoothedTargetMidi = -1.0f;
}

void QPitchAudioProcessor::resetCustomNotesToScale()
{
    for (int note = 0; note < 12; ++note)
    {
        const bool enabled = currentScaleMask[static_cast<size_t>((note - currentKey + 12) % 12)];
        if (auto* param = customNoteParams[static_cast<size_t>(note)])
            param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
    }
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

void QPitchAudioProcessor::ensureProcessingChannels(int numChannels, int numSamples)
{
    const auto channels = static_cast<size_t>(numChannels);
    if (pitchShifters.size() != channels)
    {
        pitchShifters.resize(channels);
        formantPreservers.resize(channels);
        airLpDry.assign(channels, 0.0f);
        airLpShift.assign(channels, 0.0f);
        for (size_t ch = 0; ch < channels; ++ch)
        {
            pitchShifters[ch].prepare(currentSampleRate, numSamples);
            formantPreservers[ch].prepare(currentSampleRate, numSamples, numChannels);
        }
    }

    if (dryBuffer.getNumChannels() < numChannels || dryBuffer.getNumSamples() < numSamples)
    {
        dryBuffer.setSize(numChannels, numSamples, false, false, true);
        shiftedBuffer.setSize(numChannels, numSamples, false, false, true);
        formantBuffer.setSize(numChannels, numSamples, false, false, true);
    }
}

int QPitchAudioProcessor::findNearestScaleMidi(float midiNote) const
{
    int bestMidi = static_cast<int>(std::round(midiNote));
    float bestDistance = std::numeric_limits<float>::max();

    const int center = static_cast<int>(std::round(midiNote));
    for (int candidate = center - 12; candidate <= center + 12; ++candidate)
    {
        const int noteClass = (candidate % 12 + 12) % 12;
        if (isCustomNoteEnabled(noteClass))
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

bool QPitchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() && out.isDisabled())
        return true;

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in.isDisabled())
        return true;

    if (in == juce::AudioChannelSet::mono())
        return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();

    return in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo();
}

void QPitchAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    pitchDetector.prepare(sampleRate, samplesPerBlock);
    updatePitchRange();
    const int channels = std::max(1, std::max(getTotalNumInputChannels(), getTotalNumOutputChannels()));
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
    airLpDry.assign(static_cast<size_t>(channels), 0.0f);
    airLpShift.assign(static_cast<size_t>(channels), 0.0f);
    airLpCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 4800.0f
                                   / static_cast<float>(sampleRate));

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
    std::fill(airLpDry.begin(), airLpDry.end(), 0.0f);
    std::fill(airLpShift.begin(), airLpShift.end(), 0.0f);
}

void QPitchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmax(1, totalNumInputChannels > 0 ? totalNumInputChannels : totalNumOutputChannels);

    if (numSamples <= 0 || totalNumOutputChannels <= 0)
        return;

    for (int ch = numChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    const float outputGainDb = outputGainParam->get();

    const bool correctionActive = !bypass
                                  && correctionOnParam->get()
                                  && correctionAmountParam->get() > 0.0f;

    if (!correctionActive)
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

    ensureProcessingChannels(numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

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

    const float ratio = currentPitchRatio;

  auto processMidChannel = [&](const float* dryMid, float* wetMid)
    {
        pitchShifters[0].process(dryMid, wetMid, numSamples, ratio);

        constexpr float kAirRestoreMix = 0.22f;
        float& lpDry = airLpDry[0];
        float& lpShift = airLpShift[0];
        for (int i = 0; i < numSamples; ++i)
        {
            lpDry += airLpCoeff * (dryMid[i] - lpDry);
            lpShift += airLpCoeff * (wetMid[i] - lpShift);
            wetMid[i] += kAirRestoreMix * ((dryMid[i] - lpDry) - (wetMid[i] - lpShift));
        }

        if (formantOn && formantAmount > 0.0f)
            formantPreservers[0].process(dryMid, wetMid, wetMid, numSamples, formantOn, formantAmount);
    };

    if (numChannels >= 2)
    {
        const float* dryL = dryBuffer.getReadPointer(0);
        const float* dryR = dryBuffer.getReadPointer(1);
        float* dryMid = dryBuffer.getWritePointer(0);
        float* side = formantBuffer.getWritePointer(0);
        float* wetMid = shiftedBuffer.getWritePointer(0);
        float* outL = buffer.getWritePointer(0);
        float* outR = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid = 0.5f * (dryL[i] + dryR[i]);
            dryMid[i] = mid;
            side[i] = 0.5f * (dryL[i] - dryR[i]);
        }

        processMidChannel(dryMid, wetMid);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid = wetMid[i];
            const float s = side[i];
            outL[i] = mid + s;
            outR[i] = mid - s;
        }
    }
    else
    {
        const float* dry = dryBuffer.getReadPointer(0);
        float* wet = shiftedBuffer.getWritePointer(0);
        processMidChannel(dry, wet);
        juce::FloatVectorOperations::copy(buffer.getWritePointer(0), wet, numSamples);

        if (totalNumOutputChannels > 1)
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    if (outputGainDb != 0.0f)
        buffer.applyGain(juce::Decibels::decibelsToGain(outputGainDb));
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
    if (xmlState == nullptr || !xmlState->hasTagName(vts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*xmlState);
    const bool hasNoteParams =
        state.getChildWithProperty(juce::Identifier("id"), noteParamId(0)).isValid();

    isRestoringState = true;
    vts.replaceState(state);

    currentKey = keyParam->getIndex();
    currentScale = scaleParam->getIndex();
    currentRange = rangeParam->getIndex();
    updateScaleMask();

    if (!hasNoteParams)
    {
        if (state.hasProperty("customNoteMask"))
        {
            const int maskBits = static_cast<int>(state.getProperty("customNoteMask"));
            for (int note = 0; note < 12; ++note)
            {
                if (auto* param = customNoteParams[static_cast<size_t>(note)])
                    param->setValueNotifyingHost((maskBits & (1 << note)) != 0 ? 1.0f : 0.0f);
            }
        }
        else
        {
            resetCustomNotesToScale();
        }
    }

    isRestoringState = false;
}

juce::AudioProcessorEditor* QPitchAudioProcessor::createEditor()
{
    return new QPitchAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QPitchAudioProcessor();
}
