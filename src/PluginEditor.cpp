#include "PluginEditor.h"
#include "PluginProcessor.h"

struct QPitchAudioProcessorEditor::RotarySliderWithLabel : juce::Component {
    juce::Slider slider;
    juce::Label label;
    juce::Label valueLabel;

    RotarySliderWithLabel(const juce::String& name, const juce::String& suffix)
    {
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00bcd4));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00bcd4));
        slider.setVelocityBasedMode(false);
        slider.setMouseDragSensitivity(180);
        addAndMakeVisible(slider);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(10.0f));
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
        addAndMakeVisible(valueLabel);

        slider.onValueChange = [this, suffix]() {
            float val = (float)slider.getValue();
            if (suffix == "%")
                valueLabel.setText(juce::String((int)val) + suffix, juce::dontSendNotification);
            else if (suffix == " ms")
                valueLabel.setText(juce::String(val, 1) + suffix, juce::dontSendNotification);
            else if (suffix == " dB")
                valueLabel.setText(juce::String(val, 1) + suffix, juce::dontSendNotification);
            else if (suffix == " ct")
                valueLabel.setText(juce::String((int)val) + suffix, juce::dontSendNotification);
            else
                valueLabel.setText(juce::String((int)val), juce::dontSendNotification);
        };
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromTop(18));
        auto sliderSize = juce::jmin(bounds.getWidth(), bounds.getHeight() - 16);
        slider.setBounds(bounds.withSizeKeepingCentre(sliderSize, sliderSize).reduced(2));
        valueLabel.setBounds(bounds.removeFromBottom(14));
    }
};

struct QPitchAudioProcessorEditor::PitchDisplay : juce::Component {
    float currentPitch = 0.0f;
    float targetPitch = 0.0f;
    float correctionCents = 0.0f;
    juce::String currentNote;
    juce::String targetNote;
    bool pendingRepaint = false;

    void setPitches(float detected, float target)
    {
        if (std::abs(currentPitch - detected) < 0.1f && std::abs(targetPitch - target) < 0.1f)
            return;

        currentPitch = detected;
        targetPitch = target;
        currentNote = noteNameFromFreq(detected);
        targetNote = noteNameFromFreq(target);
        pendingRepaint = true;
    }

    void setCorrectionCents(float cents)
    {
        if (std::abs(correctionCents - cents) < 0.5f && !pendingRepaint)
            return;

        correctionCents = cents;
        pendingRepaint = false;
        repaint();
    }

    static juce::String noteNameFromFreq(float freq)
    {
        if (freq <= 0) return "--";
        float midi = 69.0f + 12.0f * std::log2(freq / 440.0f);
        int note = (int)std::round(midi);
        static const char* names[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
        return names[note % 12] + juce::String(note / 12 - 1);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(4);
        g.setColour(juce::Colour(0xff1a1a2e));
        g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
        g.setColour(juce::Colour(0xff333333));
        g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.setColour(juce::Colours::lightgrey);
        g.drawText("PITCH", bounds.removeFromTop(20), juce::Justification::centred);

        auto noteBounds = bounds.reduced(4);
        auto halfW = noteBounds.getWidth() / 2;

        auto inBounds = noteBounds.removeFromLeft(halfW).reduced(2);
        g.setColour(juce::Colour(0xff00bcd4));
        g.setFont(juce::Font(22.0f, juce::Font::bold));
        g.drawText("IN", inBounds.removeFromTop(14), juce::Justification::centred);
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        if (currentPitch > 0)
        {
            g.setColour(juce::Colours::white);
            g.drawText(currentNote, inBounds.reduced(2), juce::Justification::centred);
        }

        auto outBounds = noteBounds.reduced(2);
        g.setColour(juce::Colour(0xffff6b6b));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("OUT", outBounds.removeFromTop(14), juce::Justification::centred);
        g.setFont(juce::Font(28.0f, juce::Font::bold));
        if (targetPitch > 0)
        {
            g.setColour(juce::Colours::white);
            g.drawText(targetNote, outBounds.reduced(2), juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xffff8a00));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText((correctionCents >= 0.0f ? "+" : "") + juce::String(correctionCents, 0) + " ct",
                   getLocalBounds().reduced(8).removeFromBottom(16),
                   juce::Justification::centred);
    }
};

class QPitchAudioProcessorEditor::ScaleKeyboard : public juce::Component {
public:
    explicit ScaleKeyboard(QPitchAudioProcessor& p) : processor(p) {}

    void setKeyAndScale(int newKey, int newScale)
    {
        rootKey = juce::jlimit(0, 11, newKey);
        scaleType = juce::jlimit(0, ScaleQuantizer::numScales() - 1, newScale);
        repaint();
    }

    void setLivePitches(float detectedHz, float targetHz)
    {
        const float newDetectedMidi = hzToMidi(detectedHz);
        const float newTargetMidi = hzToMidi(targetHz);

        if (std::abs(detectedMidi - newDetectedMidi) < 0.02f && std::abs(targetMidi - newTargetMidi) < 0.02f)
            return;

        detectedMidi = newDetectedMidi;
        targetMidi = newTargetMidi;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);

        g.setColour(juce::Colour(0xff121225));
        g.fillRoundedRectangle(bounds, 7.0f);
        g.setColour(juce::Colour(0xff34344a));
        g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

        auto labelArea = bounds.removeFromTop(20.0f);
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("KEYBOARD", labelArea, juce::Justification::centredLeft);
        drawLegend(g, labelArea);

        auto keyArea = bounds.reduced(4.0f, 3.0f);
        drawWhiteKeys(g, keyArea);
        drawBlackKeys(g, keyArea);
        drawLiveMarkers(g, keyArea);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const int noteClass = noteClassForPoint(event.position);
        if (noteClass >= 0)
            processor.setCustomNoteEnabled(noteClass, !processor.isCustomNoteEnabled(noteClass));
        repaint();
    }

private:
    QPitchAudioProcessor& processor;

    static constexpr int numOctaves = 2;
    static constexpr int whiteKeysPerOctave = 7;
    static constexpr int totalWhiteKeys = numOctaves * whiteKeysPerOctave;

    int rootKey = 0;
    int scaleType = 2;
    float detectedMidi = -1.0f;
    float targetMidi = -1.0f;

    static float hzToMidi(float hz)
    {
        return hz > 0.0f ? 69.0f + 12.0f * std::log2(hz / 440.0f) : -1.0f;
    }

    static int noteForWhiteKey(int whiteIndex)
    {
        static constexpr int whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11 };
        return whiteNotes[whiteIndex % whiteKeysPerOctave] + 12 * (whiteIndex / whiteKeysPerOctave);
    }

    static bool hasBlackAfterWhite(int whiteIndexInOctave)
    {
        return whiteIndexInOctave == 0 || whiteIndexInOctave == 1
            || whiteIndexInOctave == 3 || whiteIndexInOctave == 4
            || whiteIndexInOctave == 5;
    }

    bool isScaleNote(int absoluteNote) const
    {
        return processor.isCustomNoteEnabled(absoluteNote);
    }

    bool isRoot(int absoluteNote) const
    {
        return (absoluteNote % 12) == rootKey;
    }

    void drawWhiteKeys(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float keyW = area.getWidth() / static_cast<float>(totalWhiteKeys);
        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            const int note = noteForWhiteKey(i);
            auto r = juce::Rectangle<float>(area.getX() + keyW * i, area.getY(), keyW, area.getHeight());

            const bool allowed = isScaleNote(note);
            g.setColour(allowed ? juce::Colour(0xffd8fbff) : juce::Colour(0xff585861));
            g.fillRect(r.reduced(1.0f, 0.5f));

            if (isRoot(note))
            {
                g.setColour(juce::Colour(0xffff8a00));
                g.fillRect(r.withY(r.getBottom() - 9.0f).withHeight(9.0f).reduced(1.0f, 0.0f));
            }
            else if (allowed)
            {
                g.setColour(juce::Colour(0xff00bcd4));
                g.fillRect(r.withY(r.getBottom() - 7.0f).withHeight(7.0f).reduced(1.0f, 0.0f));
            }

            g.setColour(juce::Colour(0xff2b2b33));
            g.drawRect(r, 1.0f);
        }
    }

    void drawBlackKeys(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float keyW = area.getWidth() / static_cast<float>(totalWhiteKeys);
        const float blackW = keyW * 0.58f;
        const float blackH = area.getHeight() * 0.62f;

        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            const int whiteInOctave = i % whiteKeysPerOctave;
            if (!hasBlackAfterWhite(whiteInOctave))
                continue;

            const int note = noteForWhiteKey(i) + 1;
            const float centerX = area.getX() + keyW * (static_cast<float>(i) + 1.0f);
            auto r = juce::Rectangle<float>(centerX - blackW * 0.5f, area.getY(), blackW, blackH);

            const bool allowed = isScaleNote(note);
            g.setColour(allowed ? juce::Colour(0xff053b44) : juce::Colour(0xff151519));
            g.fillRoundedRectangle(r, 2.5f);

            if (isRoot(note))
            {
                g.setColour(juce::Colour(0xffff8a00));
                g.fillRoundedRectangle(r.withY(r.getBottom() - 8.0f).withHeight(8.0f), 2.0f);
            }
            else if (allowed)
            {
                g.setColour(juce::Colour(0xff00bcd4));
                g.fillRoundedRectangle(r.withY(r.getBottom() - 7.0f).withHeight(7.0f), 2.0f);
            }

            g.setColour(juce::Colour(0xff22222c));
            g.drawRoundedRectangle(r, 2.5f, 1.0f);
        }
    }

    float xForMidi(juce::Rectangle<float> area, float midi) const
    {
        if (midi < 0.0f)
            return -1.0f;

        static constexpr int whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23 };
        const float wrapped = std::fmod(midi + 120.0f, 24.0f);

        int lowerWhite = 0;
        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            if (static_cast<float>(whiteNotes[i]) <= wrapped)
                lowerWhite = i;
        }

        const float keyW = area.getWidth() / static_cast<float>(totalWhiteKeys);
        const int nextWhite = std::min(lowerWhite + 1, totalWhiteKeys - 1);
        const float span = std::max(1.0f, static_cast<float>(whiteNotes[nextWhite] - whiteNotes[lowerWhite]));
        const float frac = std::clamp((wrapped - static_cast<float>(whiteNotes[lowerWhite])) / span, 0.0f, 1.0f);
        return area.getX() + (static_cast<float>(lowerWhite) + frac) * keyW;
    }

    void drawLiveMarkers(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float inX = xForMidi(area, detectedMidi);
        const float outX = xForMidi(area, targetMidi);

        if (inX >= 0.0f)
        {
            g.setColour(juce::Colour(0xffff8a00));
            g.fillEllipse(inX - 5.0f, area.getY() + 8.0f, 10.0f, 10.0f);
            g.drawLine(inX, area.getY() + 20.0f, inX, area.getBottom() - 5.0f, 2.0f);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("IN", static_cast<int>(inX - 12.0f), static_cast<int>(area.getY() + 20.0f), 24, 14,
                       juce::Justification::centred);
        }

        if (outX >= 0.0f)
        {
            g.setColour(juce::Colour(0xffff2bd6));
            g.fillEllipse(outX - 6.0f, area.getBottom() - 19.0f, 12.0f, 12.0f);
            g.drawLine(outX, area.getY() + 5.0f, outX, area.getBottom() - 21.0f, 2.5f);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("OUT", static_cast<int>(outX - 16.0f), static_cast<int>(area.getBottom() - 34.0f), 32, 14,
                       juce::Justification::centred);
        }
    }

    void drawLegend(juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto x = area.getRight() - 260.0f;
        const auto y = area.getY() + 4.0f;
        const auto item = [&](juce::Colour colour, const char* text, float width)
        {
            g.setColour(colour);
            g.fillRoundedRectangle(x, y + 3.0f, 12.0f, 8.0f, 2.0f);
            g.setColour(juce::Colour(0xffbfc1cb));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(text, static_cast<int>(x + 16.0f), static_cast<int>(y), static_cast<int>(width), 14,
                       juce::Justification::centredLeft);
            x += width + 22.0f;
        };

        item(juce::Colour(0xff00bcd4), "ON", 24.0f);
        item(juce::Colour(0xff585861), "IGNORED", 56.0f);
        item(juce::Colour(0xffff8a00), "ROOT/IN", 52.0f);
        item(juce::Colour(0xffff2bd6), "OUT", 30.0f);
    }

    int noteClassForPoint(juce::Point<float> point) const
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        bounds.removeFromTop(20.0f);
        auto area = bounds.reduced(4.0f, 3.0f);
        if (!area.contains(point))
            return -1;

        const float keyW = area.getWidth() / static_cast<float>(totalWhiteKeys);
        const float blackW = keyW * 0.58f;
        const float blackH = area.getHeight() * 0.62f;

        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            const int whiteInOctave = i % whiteKeysPerOctave;
            if (!hasBlackAfterWhite(whiteInOctave))
                continue;

            const float centerX = area.getX() + keyW * (static_cast<float>(i) + 1.0f);
            auto r = juce::Rectangle<float>(centerX - blackW * 0.5f, area.getY(), blackW, blackH);
            if (r.contains(point))
                return (noteForWhiteKey(i) + 1) % 12;
        }

        const int white = juce::jlimit(0, totalWhiteKeys - 1, static_cast<int>((point.x - area.getX()) / keyW));
        return noteForWhiteKey(white) % 12;
    }
};

QPitchAudioProcessorEditor::QPitchAudioProcessorEditor(QPitchAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    auto& vts = processor.getValueTreeState();

    retuneSpeedSlider = std::make_unique<RotarySliderWithLabel>("Speed", " ms");
    addAndMakeVisible(*retuneSpeedSlider);
    retuneAttach = std::make_unique<SliderAttach>(vts, "retune_speed", retuneSpeedSlider->slider);

    noteTransitionSlider = std::make_unique<RotarySliderWithLabel>("Transition", " ms");
    addAndMakeVisible(*noteTransitionSlider);
    noteTransitionAttach = std::make_unique<SliderAttach>(vts, "note_transition", noteTransitionSlider->slider);

    correctionAmountSlider = std::make_unique<RotarySliderWithLabel>("Correction", "%");
    addAndMakeVisible(*correctionAmountSlider);
    correctionAttach = std::make_unique<SliderAttach>(vts, "correction_amount", correctionAmountSlider->slider);

    humanizeSlider = std::make_unique<RotarySliderWithLabel>("Humanize", " ct");
    addAndMakeVisible(*humanizeSlider);
    humanizeAttach = std::make_unique<SliderAttach>(vts, "humanize", humanizeSlider->slider);

    outputGainSlider = std::make_unique<RotarySliderWithLabel>("Gain", " dB");
    addAndMakeVisible(*outputGainSlider);
    outputGainAttach = std::make_unique<SliderAttach>(vts, "output_gain", outputGainSlider->slider);

    keyCombo = std::make_unique<juce::ComboBox>();
    for (int i = 0; i < 12; ++i)
        keyCombo->addItem(ScaleQuantizer::getKeyName(i), i + 1);
    keyCombo->setSelectedItemIndex(0);
    keyCombo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a2e));
    keyCombo->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    keyCombo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333333));
    keyCombo->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff00bcd4));
    addAndMakeVisible(*keyCombo);
    keyAttach = std::make_unique<ComboAttach>(vts, "key", *keyCombo);

    scaleCombo = std::make_unique<juce::ComboBox>();
    for (int i = 0; i < ScaleQuantizer::numScales(); ++i)
        scaleCombo->addItem(ScaleQuantizer::getScaleName(i), i + 1);
    scaleCombo->setSelectedItemIndex(2);
    scaleCombo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a2e));
    scaleCombo->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    scaleCombo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333333));
    scaleCombo->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff00bcd4));
    addAndMakeVisible(*scaleCombo);
    scaleAttach = std::make_unique<ComboAttach>(vts, "scale", *scaleCombo);

    rangeCombo = std::make_unique<juce::ComboBox>();
    rangeCombo->addItemList(juce::StringArray { "Bass", "Baritone", "Tenor", "Alto", "Mezzo Soprano", "Soprano", "Generic" }, 1);
    rangeCombo->setSelectedItemIndex(6);
    rangeCombo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a2e));
    rangeCombo->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    rangeCombo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff333333));
    rangeCombo->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff00bcd4));
    addAndMakeVisible(*rangeCombo);
    rangeAttach = std::make_unique<ComboAttach>(vts, "range", *rangeCombo);

    formantToggle = std::make_unique<juce::ToggleButton>("Formant: Corrected");
    formantToggle->setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    formantToggle->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00bcd4));
    addAndMakeVisible(*formantToggle);
    formantAttach = std::make_unique<ButtonAttach>(vts, "formant_on", *formantToggle);

    resetScaleButton = std::make_unique<juce::TextButton>("Reset Notes");
    resetScaleButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a2e));
    resetScaleButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    resetScaleButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00bcd4));
    resetScaleButton->onClick = [this]()
    {
        processor.resetCustomNotesToScale();
        if (scaleKeyboard != nullptr)
            scaleKeyboard->repaint();
    };
    addAndMakeVisible(*resetScaleButton);

    pitchDisplay = std::make_unique<PitchDisplay>();
    addAndMakeVisible(*pitchDisplay);

    scaleKeyboard = std::make_unique<ScaleKeyboard>(processor);
    addAndMakeVisible(*scaleKeyboard);

    auto refreshKeyboard = [this]()
    {
        processor.resetCustomNotesToScale();
        if (scaleKeyboard != nullptr && keyCombo != nullptr && scaleCombo != nullptr)
            scaleKeyboard->setKeyAndScale(keyCombo->getSelectedItemIndex(), scaleCombo->getSelectedItemIndex());
    };
    keyCombo->onChange = refreshKeyboard;
    scaleCombo->onChange = refreshKeyboard;
    refreshKeyboard();

    setResizable(true, true);
    setResizeLimits(620, 540, 980, 760);
    setSize(620, 540);
    startTimerHz(24);
}

QPitchAudioProcessorEditor::~QPitchAudioProcessorEditor() = default;

void QPitchAudioProcessorEditor::timerCallback()
{
    const float detectedHz = processor.getDebugDetectedHz();
    const float targetHz = processor.getDebugTargetHz();

    if (pitchDisplay != nullptr)
    {
        pitchDisplay->setPitches(detectedHz, targetHz);
        pitchDisplay->setCorrectionCents(processor.getDebugCorrectionCents());
    }

    if (scaleKeyboard != nullptr)
        scaleKeyboard->setLivePitches(detectedHz, targetHz);
}

void QPitchAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    auto topBar = bounds.removeFromTop(40);

    auto titleBounds = topBar.removeFromLeft(140);
    auto pitchBounds = topBar;

    auto mainArea = bounds.removeFromTop(180);

    auto sliderRow = mainArea.removeFromTop(120);
    auto sliderW = sliderRow.getWidth() / 5;
    retuneSpeedSlider->setBounds(sliderRow.removeFromLeft(sliderW).reduced(3));
    noteTransitionSlider->setBounds(sliderRow.removeFromLeft(sliderW).reduced(3));
    correctionAmountSlider->setBounds(sliderRow.removeFromLeft(sliderW).reduced(3));
    humanizeSlider->setBounds(sliderRow.removeFromLeft(sliderW).reduced(3));
    outputGainSlider->setBounds(sliderRow.reduced(3));

    pitchDisplay->setBounds(pitchBounds);

    auto bottomArea = bounds;
    auto bottomRow = bottomArea.removeFromTop(38);

    auto comboW = bottomRow.getWidth() / 4;
    auto keyLabel = bottomRow.removeFromLeft(comboW).reduced(4);
    keyCombo->setBounds(keyLabel);

    auto scaleLabel = bottomRow.removeFromLeft(comboW).reduced(4);
    scaleCombo->setBounds(scaleLabel);

    auto resetLabel = bottomRow.removeFromLeft(comboW).reduced(4);
    resetScaleButton->setBounds(resetLabel);

    auto toggleLabel = bottomRow.reduced(4);
    formantToggle->setBounds(toggleLabel);

    auto rangeRow = bottomArea.removeFromTop(42);
    rangeCombo->setBounds(rangeRow.reduced(4));

    bottomArea.removeFromTop(4);
    scaleKeyboard->setBounds(bottomArea.reduced(0, 2));
}

void QPitchAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f0f23));

    g.setColour(juce::Colour(0xff00bcd4));
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("QPitch", 12, 8, 100, 30, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffff8a00));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("v2 KEYBOARD", getWidth() - 116, 10, 104, 22, juce::Justification::centredRight);

    g.setColour(juce::Colour(0xff00bcd4));
    g.fillRoundedRectangle(10, 35, getWidth() - 20, 1, 0.5f);
}
