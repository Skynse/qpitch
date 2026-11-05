#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <limits>

static const juce::String paramRetuneSpeed = "retune_speed";
static const juce::String paramNoteTransition = "note_transition";
static const juce::String paramCorrectionOn = "correction_on";
static const juce::String paramCorrectionAmount = "correction_amount";
static const juce::String paramToleranceCents = "tolerance_cents";
static const juce::String paramTPain = "tpain";
static const juce::String paramReferenceFrequency = "reference_frequency";
static const juce::String paramKey = "key";
static const juce::String paramScale = "scale";
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
    tPainParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramTPain));
    referenceFrequencyParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramReferenceFrequency));
    keyParam = static_cast<juce::AudioParameterChoice*>(vts.getParameter(paramKey));
    scaleParam = static_cast<juce::AudioParameterChoice*>(vts.getParameter(paramScale));
    formantOnParam = static_cast<juce::AudioParameterBool*>(vts.getParameter(paramFormantOn));
    humanizeParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramHumanize));
    outputGainParam = static_cast<juce::AudioParameterFloat*>(vts.getParameter(paramOutputGain));
    for (int note = 0; note < 12; ++note)
        customNoteParams[static_cast<size_t>(note)] =
            static_cast<juce::AudioParameterBool*>(vts.getParameter(noteParamId(note)));

    vts.addParameterListener(paramReferenceFrequency, this);
    vts.addParameterListener(paramKey, this);
    vts.addParameterListener(paramScale, this);

    currentKey = keyParam->getIndex();
    currentScale = scaleParam->getIndex();
    updateScaleMask();
    resetCustomNotesToScale();
}

QPitchAudioProcessor::~QPitchAudioProcessor()
{
    vts.removeParameterListener(paramReferenceFrequency, this);
    vts.removeParameterListener(paramKey, this);
    vts.removeParameterListener(paramScale, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout QPitchAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params; //parameter vec

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramRetuneSpeed, "Retune Speed",
        juce::NormalisableRange<float>(0.0f, 800.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramNoteTransition, "Note Transition",
        juce::NormalisableRange<float>(0.0f, 800.0f, 0.1f), 60.0f,
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

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        paramFormantOn, "Formant On", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        paramHumanize, "Humanize",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 0) + " %"; })));

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
    if (parameterID == paramKey)
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
    else if (parameterID == paramReferenceFrequency)
    {
        scaleQuantizer.setReferenceFrequency(newValue);
        lockedTargetMidi = -1;
        smoothedInputMidi = -1.0f;
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
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
    lockedTargetMidi = -1;
}

void QPitchAudioProcessor::resetCustomNotesToScale()
{
    for (int note = 0; note < 12; ++note)
    {
        const bool enabled = currentScaleMask[static_cast<size_t>((note - currentKey + 12) % 12)];
        if (auto* param = customNoteParams[static_cast<size_t>(note)])
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    }
    lockedTargetMidi = -1;
}

void QPitchAudioProcessor::ensureProcessingChannels(int numChannels, int numSamples)
{
    if (pitchShifter.getNumChannels() != numChannels)
    {
        pitchShifter.prepare(currentSampleRate, numSamples, numChannels);
        dryDelay.setSize(numChannels, getLatencySamples());
        dryDelay.clear();
        dryDelayPosition = 0;
    }

    if (dryBuffer.getNumChannels() < numChannels || dryBuffer.getNumSamples() < numSamples)
    {
        dryBuffer.setSize(numChannels, numSamples, false, false, true);
        shiftedBuffer.setSize(numChannels, numSamples, false, false, true);
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
    outputPitchDetector.prepare(sampleRate, samplesPerBlock);
    pitchDetector.setFrequencyRange(55.0f, 1000.0f);
    outputPitchDetector.setFrequencyRange(55.0f, 1000.0f);
    const int channels = std::max(1, getTotalNumInputChannels());
    pitchShifter.prepare(sampleRate, samplesPerBlock, channels);
    dryBuffer.setSize(channels, samplesPerBlock, false, true, true);
    shiftedBuffer.setSize(channels, samplesPerBlock, false, true, true);

    setLatencySamples(pitchShifter.getLatencySamples());
    dryDelay.setSize(channels, getLatencySamples());
    dryDelay.clear();
    dryDelayPosition = 0;

    currentWetMix = 0.0f;
    controlCountdown = controlInterval;
    resetTracking();
    scaleQuantizer.setReferenceFrequency(referenceFrequencyParam->get());
}

void QPitchAudioProcessor::resetTracking()
{
    noteCorrectionCents = 0.0f;
    transitionCents = 0.0f;
    transitionStartCents = 0.0f;
    smoothedInputMidi = -1.0f;
    lastCorrectionCents = 0.0f;
    lockedTargetMidi = -1;
    noteHeldSamples = 0;
    pitchHoldSamples = 0;
}

void QPitchAudioProcessor::releaseResources()
{
    pitchDetector.reset();
    outputPitchDetector.reset();
    outputPitchHz.store(0.0f);
    dryDelay.clear();
    dryDelayPosition = 0;
    currentWetMix = 0.0f;
    pitchShifter.reset();
}

float QPitchAudioProcessor::updateCorrection(const float* input, int numSamples)
{
    const float sampleRate = static_cast<float>(currentSampleRate);
    const float detectedHz = pitchDetector.detectPitch(input, numSamples);
    const bool pitchValid = detectedHz > 0.0f && pitchDetector.getConfidence() > 0.62f;
    auto coefficient = [&](float ms) {
        return ms <= 0.0f ? 0.0f : std::exp(-static_cast<float>(numSamples) / (ms * 0.001f * sampleRate));
    };

    if (!pitchValid)
    {
        if (pitchHoldSamples > 0)
        {
            pitchHoldSamples = std::max(0, pitchHoldSamples - numSamples);
            return lastCorrectionCents;
        }

        lockedTargetMidi = -1;
        smoothedInputMidi = -1.0f;
        const float release = coefficient(30.0f);
        noteCorrectionCents *= release;
        transitionCents *= release;
        lastCorrectionCents *= release;
        if (std::abs(lastCorrectionCents) < 1.0f)
        {
            detectedPitchHz.store(0.0f);
            targetPitchHz.store(0.0f);
        }
        return lastCorrectionCents;
    }

    // T-Pain moves both time controls toward the instant, hard-tuned extreme.
    const float tPain = tPainParam->get() / 100.0f;
    const float retuneMs = retuneSpeedParam->get() * (1.0f - tPain);
    const float transitionMs = noteTransitionParam->get() * (1.0f - tPain);
    const float toleranceCents = toleranceCentsParam->get();
    const float humanize = humanizeParam->get() / 100.0f;

    // Fold octave jumps into the current octave: correction only depends on pitch class,
    // and this keeps detector octave errors from moving the target.
    float inputMidi = scaleQuantizer.hzToMidi(detectedHz);
    if (smoothedInputMidi >= 0.0f)
    {
        while (inputMidi - smoothedInputMidi > 6.0f)
            inputMidi -= 12.0f;
        while (smoothedInputMidi - inputMidi > 6.0f)
            inputMidi += 12.0f;
    }
    else
    {
        smoothedInputMidi = inputMidi;
    }
    const float followCoeff = coefficient(45.0f);
    smoothedInputMidi = smoothedInputMidi * followCoeff + inputMidi * (1.0f - followCoeff);

    const int nearestMidi = findNearestScaleMidi(inputMidi);
    if (lockedTargetMidi < 0)
    {
        lockedTargetMidi = nearestMidi;
        transitionCents = 0.0f;
        noteHeldSamples = 0;
    }
    else if (nearestMidi != lockedTargetMidi)
    {
        // Switch once the voice is clearly past the midpoint between the two notes.
        constexpr float hysteresis = 0.15f;
        const float currentDistance = std::abs(inputMidi - static_cast<float>(lockedTargetMidi));
        const float newDistance = std::abs(inputMidi - static_cast<float>(nearestMidi));
        if (newDistance + hysteresis < currentDistance)
        {
            // The note step bypasses Retune Speed; Note Transition alone decides how fast
            // the output travels to the new note.
            const float stepCents = static_cast<float>(nearestMidi - lockedTargetMidi) * 100.0f;
            noteCorrectionCents += stepCents;
            transitionCents -= stepCents;
            transitionStartCents = std::abs(transitionCents);
            lockedTargetMidi = nearestMidi;
            noteHeldSamples = 0;
        }
    }

    float desiredCents = (static_cast<float>(lockedTargetMidi) - inputMidi) * 100.0f;
    if (toleranceCents > 0.0f)
        desiredCents = std::abs(desiredCents) <= toleranceCents
            ? 0.0f
            : desiredCents - std::copysign(toleranceCents, desiredCents);

    // Humanize slows retuning as a note is sustained, so held notes keep their vibrato
    // while short notes still snap.
    const float sustain = std::min(1.0f, static_cast<float>(noteHeldSamples) / (0.25f * sampleRate));
    const float noteCoeff = coefficient(retuneMs + humanize * sustain * 300.0f);
    noteCorrectionCents = noteCorrectionCents * noteCoeff + desiredCents * (1.0f - noteCoeff);
    noteHeldSamples += numSamples;

    if (transitionCents != 0.0f)
    {
        if (transitionMs <= 0.0f)
        {
            transitionCents = 0.0f;
        }
        else
        {
            const float step = transitionStartCents * static_cast<float>(numSamples) / (transitionMs * 0.001f * sampleRate);
            transitionCents = transitionCents > 0.0f ? std::max(0.0f, transitionCents - step)
                                                     : std::min(0.0f, transitionCents + step);
        }
        // Never hold the voice further back than the singer already is.
        const float sungCents = (inputMidi - static_cast<float>(lockedTargetMidi)) * 100.0f;
        transitionCents = transitionCents < 0.0f ? std::max(transitionCents, std::min(sungCents, 0.0f))
                                                 : std::min(transitionCents, std::max(sungCents, 0.0f));
    }

    lastCorrectionCents = std::clamp((noteCorrectionCents + transitionCents) * correctionAmountParam->get() / 100.0f,
                                     -1200.0f, 1200.0f);
    pitchHoldSamples = static_cast<int>(0.150f * sampleRate);

    // Report the target in the octave the detector heard, not the folded octave.
    const float targetOffset = static_cast<float>(lockedTargetMidi) + transitionCents / 100.0f - inputMidi;
    detectedPitchHz.store(detectedHz);
    targetPitchHz.store(detectedHz * std::pow(2.0f, targetOffset / 12.0f));
    return lastCorrectionCents;
}

void QPitchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmax(1, totalNumInputChannels > 0 ? totalNumInputChannels : totalNumOutputChannels);
    jassert(numChannels <= 2);

    if (numSamples <= 0 || totalNumOutputChannels <= 0)
        return;

    for (int ch = numChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    const float outputGainDb = outputGainParam->get();

    const bool hasAllowedNotes = std::any_of(customNoteParams.begin(), customNoteParams.end(),
        [](const auto* note) { return note != nullptr && note->get(); });
    const bool correctionActive = !bypass
                                  && correctionOnParam->get()
                                  && correctionAmountParam->get() > 0.0f
                                  && hasAllowedNotes;

    scaleQuantizer.setReferenceFrequency(referenceFrequencyParam->get());
    const bool formantOn = formantOnParam->get();

    ensureProcessingChannels(numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Keep the live engine running even while correction is off. Crossfade to
    // an exactly delayed dry signal so bypass never changes host alignment.
    for (int offset = 0; offset < numSamples;)
    {
        const int count = std::min(numSamples - offset, controlCountdown);
        const float correctionCents = updateCorrection(dryBuffer.getReadPointer(0, offset), count);
        controlCountdown -= count;
        if (controlCountdown == 0)
            controlCountdown = controlInterval;

        const float ratio = correctionActive ? std::clamp(std::pow(2.0f, correctionCents / 1200.0f), 0.5f, 2.0f)
                                             : 1.0f;
        std::array<const float*, 2> input {};
        std::array<float*, 2> output {};
        for (int ch = 0; ch < numChannels; ++ch)
        {
            input[static_cast<size_t>(ch)] = dryBuffer.getReadPointer(ch, offset);
            output[static_cast<size_t>(ch)] = shiftedBuffer.getWritePointer(ch, offset);
        }
        pitchShifter.process(input.data(), output.data(), count, ratio, formantOn);
        offset += count;
    }

    const float wetTarget = correctionActive ? 1.0f : 0.0f;
    const float fadeStep = 1.0f / std::max(1.0f, static_cast<float>(currentSampleRate) * 0.005f);
    for (int i = 0; i < numSamples; ++i)
    {
        currentWetMix += std::clamp(wetTarget - currentWetMix, -fadeStep, fadeStep);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float delayed = dryDelay.getSample(ch, dryDelayPosition);
            dryDelay.setSample(ch, dryDelayPosition, dryBuffer.getSample(ch, i));
            buffer.setSample(ch, i, delayed + currentWetMix * (shiftedBuffer.getSample(ch, i) - delayed));
        }
        if (++dryDelayPosition == dryDelay.getNumSamples()) dryDelayPosition = 0;
    }
    if (!correctionActive)
        targetPitchHz.store(0.0f);

    for (int ch = numChannels; ch < totalNumOutputChannels; ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);

    const float outputHz = outputPitchDetector.detectPitch(buffer.getReadPointer(0), numSamples);
    outputPitchHz.store(outputPitchDetector.getConfidence() > 0.62f ? outputHz : 0.0f);

    if (outputGainDb != 0.0f)
        buffer.applyGain(juce::Decibels::decibelsToGain(outputGainDb));
}

void QPitchAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    bypass = true;
    processBlock(buffer, midi);
    bypass = false;
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
