#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

#include "dsp/PitchDetector.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/PitchShifter.h"

class QPitchAudioProcessor final : public juce::AudioProcessor,
                                   public juce::AudioProcessorValueTreeState::Listener
{
public:
    QPitchAudioProcessor();
    ~QPitchAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QPitch"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return vts; }
    float getDetectedHz() const { return detectedPitchHz.load(); }
    float getOutputHz() const { return outputPitchHz.load(); }
    float getTargetHz() const { return targetPitchHz.load(); }
    float getReferenceFrequency() const { return scaleQuantizer.getReferenceFrequency(); }
    bool isCustomNoteEnabled(int noteClass) const;
    void setCustomNoteEnabled(int noteClass, bool enabled);
    void resetCustomNotesToScale();

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateScaleMask();
    void updatePitchRange();
    void ensureProcessingChannels(int numChannels, int numSamples);
    int findNearestScaleMidi(float midiNote) const;

    juce::AudioProcessorValueTreeState vts;

    juce::AudioParameterFloat* retuneSpeedParam = nullptr;
    juce::AudioParameterFloat* noteTransitionParam = nullptr;
    juce::AudioParameterBool* correctionOnParam = nullptr;
    juce::AudioParameterFloat* correctionAmountParam = nullptr;
    juce::AudioParameterFloat* toleranceCentsParam = nullptr;
    juce::AudioParameterFloat* toleranceTimeParam = nullptr;
    juce::AudioParameterFloat* snappinessParam = nullptr;
    juce::AudioParameterFloat* tPainParam = nullptr;
    juce::AudioParameterFloat* referenceFrequencyParam = nullptr;
    juce::AudioParameterChoice* keyParam = nullptr;
    juce::AudioParameterChoice* scaleParam = nullptr;
    juce::AudioParameterChoice* rangeParam = nullptr;
    juce::AudioParameterBool* formantOnParam = nullptr;
    juce::AudioParameterFloat* humanizeParam = nullptr;
    juce::AudioParameterFloat* outputGainParam = nullptr;
    juce::AudioParameterFloat* pitchDetectParam = nullptr;

    PitchDetector pitchDetector;
    PitchDetector outputPitchDetector;
    ScaleQuantizer scaleQuantizer;
    PitchShifter pitchShifter;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> shiftedBuffer;

    double currentSampleRate = 44100.0;
    float pitchCoefficient = 0.0f;
    float currentPitchRatio = 1.0f;
    float humanizePhase = 0.0f;
    float currentWetMix = 0.0f;
    float smoothedCorrectionCents = 0.0f;
    float smoothedTargetMidi = -1.0f;
    float smoothedInputMidi = -1.0f;
    int lockedTargetMidi = -1;
    int pendingTargetMidi = -1;
    int pendingTargetSamples = 0;
    int pitchHoldSamples = 0;
    int currentKey = 0;
    int currentScale = 0;
    int currentRange = 6;
    bool bypass = false;
    juce::AudioBuffer<float> dryDelay;
    int dryDelayPosition = 0;
    std::atomic<float> outputPitchHz { 0.0f };
    std::atomic<float> detectedPitchHz { 0.0f };
    std::atomic<float> targetPitchHz { 0.0f };

    std::array<bool, 12> currentScaleMask;
    std::array<juce::AudioParameterBool*, 12> customNoteParams {};
    bool isRestoringState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QPitchAudioProcessor)
};
