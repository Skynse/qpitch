#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

#include "dsp/PitchDetector.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/PitchShifter.h"
#include "dsp/FormantPreserver.h"

class QPitchAudioProcessor final : public juce::AudioProcessor,
                                   public juce::AudioProcessorValueTreeState::Listener
{
public:
    QPitchAudioProcessor();
    ~QPitchAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    float getDebugDetectedHz() const { return debugDetectedHz.load(); }
    float getDebugTargetHz() const { return debugTargetHz.load(); }
    float getDebugCorrectionCents() const { return debugCorrectionCents.load(); }
    float getReferenceFrequency() const { return scaleQuantizer.getReferenceFrequency(); }
    bool isCustomNoteEnabled(int noteClass) const;
    void setCustomNoteEnabled(int noteClass, bool enabled);
    void resetCustomNotesToScale();

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void smoothPitch(float targetHz, int numSamples);
    void updateScaleMask();
    void updatePitchRange();
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
    ScaleQuantizer scaleQuantizer;
    std::vector<PitchShifter> pitchShifters;
    std::vector<FormantPreserver> formantPreservers;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> shiftedBuffer;
    juce::AudioBuffer<float> formantBuffer;

    double currentSampleRate = 44100.0;
    float currentSmoothedPitch = 0.0f;
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
    bool wasCorrectionActive = false;
    std::atomic<float> debugDetectedHz { 0.0f };
    std::atomic<float> debugTargetHz { 0.0f };
    std::atomic<float> debugCorrectionCents { 0.0f };

    std::array<bool, 12> currentScaleMask;
    std::array<std::atomic<bool>, 12> customNoteMask;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QPitchAudioProcessor)
};
