#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

class QPitchAudioProcessor;

class QPitchAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer {
public:
    explicit QPitchAudioProcessorEditor(QPitchAudioProcessor&);
    ~QPitchAudioProcessorEditor() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    void timerCallback();

private:
    class RotarySliderWithLabel;
    class PitchDisplay;
    class ScaleKeyboard;

    QPitchAudioProcessor& processor;

    std::unique_ptr<RotarySliderWithLabel> retuneSpeedSlider;
    std::unique_ptr<RotarySliderWithLabel> noteTransitionSlider;
    std::unique_ptr<RotarySliderWithLabel> correctionAmountSlider;
    std::unique_ptr<RotarySliderWithLabel> toleranceCentsSlider;
    std::unique_ptr<RotarySliderWithLabel> toleranceTimeSlider;
    std::unique_ptr<RotarySliderWithLabel> snappinessSlider;
    std::unique_ptr<RotarySliderWithLabel> tPainSlider;
    std::unique_ptr<RotarySliderWithLabel> referenceSlider;
    std::unique_ptr<RotarySliderWithLabel> humanizeSlider;
    std::unique_ptr<RotarySliderWithLabel> outputGainSlider;

    std::unique_ptr<juce::ComboBox> keyCombo;
    std::unique_ptr<juce::ComboBox> scaleCombo;
    std::unique_ptr<juce::ComboBox> rangeCombo;
    std::unique_ptr<juce::ToggleButton> correctionToggle;
    std::unique_ptr<juce::ToggleButton> formantToggle;
    std::unique_ptr<juce::TextButton> resetScaleButton;

    std::unique_ptr<PitchDisplay> pitchDisplay;
    std::unique_ptr<ScaleKeyboard> scaleKeyboard;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttach> retuneAttach;
    std::unique_ptr<SliderAttach> noteTransitionAttach;
    std::unique_ptr<SliderAttach> correctionAttach;
    std::unique_ptr<SliderAttach> toleranceCentsAttach;
    std::unique_ptr<SliderAttach> toleranceTimeAttach;
    std::unique_ptr<SliderAttach> snappinessAttach;
    std::unique_ptr<SliderAttach> tPainAttach;
    std::unique_ptr<SliderAttach> referenceAttach;
    std::unique_ptr<SliderAttach> humanizeAttach;
    std::unique_ptr<SliderAttach> outputGainAttach;
    std::unique_ptr<ComboAttach> keyAttach;
    std::unique_ptr<ComboAttach> scaleAttach;
    std::unique_ptr<ComboAttach> rangeAttach;
    std::unique_ptr<ButtonAttach> correctionOnAttach;
    std::unique_ptr<ButtonAttach> formantAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QPitchAudioProcessorEditor)
};
