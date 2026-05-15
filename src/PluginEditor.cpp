#include "PluginEditor.h"
#include "PluginProcessor.h"

// ─── Custom LookAndFeel ───────────────────────────────────────────────────────

class QPitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QPitchLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff181828));
        setColour(juce::PopupMenu::textColourId,                  juce::Colours::lightgrey);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff00bcd4).withAlpha(0.25f));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        const float outerR   = juce::jmin(width * 0.5f, height * 0.5f) - 3.0f;
        const float innerR   = outerR * 0.65f;
        const float cx       = x + width  * 0.5f;
        const float cy       = y + height * 0.5f;
        const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Filled knob body
        g.setColour(juce::Colour(0xff22223a));
        g.fillEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);

        // Track
        {
            juce::Path track;
            track.addArc(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colour(0xff2c2c44));
            g.strokePath(track, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // Value arc
        {
            juce::Path val;
            val.addArc(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f,
                       rotaryStartAngle, angle, true);
            g.setColour(juce::Colour(0xff00bcd4));
            g.strokePath(val, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        // Inner circle
        g.setColour(juce::Colour(0xff1c1c2e));
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
        g.setColour(juce::Colour(0xff353550));
        g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

        // Pointer
        const float pLen = innerR * 0.65f;
        const float px   = cx + pLen * std::sin(angle);
        const float py   = cy - pLen * std::cos(angle);
        g.setColour(juce::Colour(0xff00e5ff));
        g.drawLine(cx, cy, px, py, 2.0f);
        g.fillEllipse(px - 2.5f, py - 2.5f, 5.0f, 5.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int, int, int, int, juce::ComboBox&) override
    {
        juce::Rectangle<float> b(0.5f, 0.5f, width - 1.0f, height - 1.0f);
        g.setColour(juce::Colour(0xff181828));
        g.fillRoundedRectangle(b, 5.0f);
        g.setColour(isButtonDown ? juce::Colour(0xff00bcd4) : juce::Colour(0xff3a3a5c));
        g.drawRoundedRectangle(b, 5.0f, 1.0f);

        const float ax = width - 18.0f;
        const float ay = height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(ax,        ay - 3.0f);
        arrow.lineTo         (ax + 6.0f, ay + 3.0f);
        arrow.lineTo         (ax + 12.0f, ay - 3.0f);
        g.setColour(juce::Colour(0xff00bcd4));
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(12.0f);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(8, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool, bool) override
    {
        const bool on = button.getToggleState();
        auto b = button.getLocalBounds().toFloat();

        const float ph = 16.0f, pw = 32.0f, px = 4.0f;
        const float py = (b.getHeight() - ph) * 0.5f;
        juce::Rectangle<float> pill(px, py, pw, ph);

        g.setColour(on ? juce::Colour(0xff004d5a) : juce::Colour(0xff1c1c2e));
        g.fillRoundedRectangle(pill, ph * 0.5f);
        g.setColour(on ? juce::Colour(0xff00bcd4) : juce::Colour(0xff3a3a5c));
        g.drawRoundedRectangle(pill, ph * 0.5f, 1.0f);

        const float tr = ph * 0.36f;
        const float tx = on ? pill.getRight() - tr - 3.0f : pill.getX() + tr + 3.0f;
        g.setColour(on ? juce::Colour(0xff00bcd4) : juce::Colour(0xff484862));
        g.fillEllipse(tx - tr, pill.getCentreY() - tr, tr * 2.0f, tr * 2.0f);

        g.setColour(on ? juce::Colours::white : juce::Colour(0xff888899));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(button.getButtonText(),
                   juce::Rectangle<float>(px + pw + 7.0f, 0.0f, b.getWidth() - pw - 11.0f, b.getHeight()),
                   juce::Justification::centredLeft, false);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool isHighlighted, bool isDown) override
    {
        auto b = button.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(isDown       ? juce::Colour(0xff00bcd4).withAlpha(0.35f)
                  : isHighlighted ? juce::Colour(0xff252540)
                                  : juce::Colour(0xff181828));
        g.fillRoundedRectangle(b, 5.0f);
        g.setColour((isHighlighted || isDown) ? juce::Colour(0xff00bcd4) : juce::Colour(0xff3a3a5c));
        g.drawRoundedRectangle(b, 5.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, false);
    }
};

// ─── RotarySliderWithLabel ────────────────────────────────────────────────────

struct QPitchAudioProcessorEditor::RotarySliderWithLabel : juce::Component {
    juce::Slider slider;
    juce::Label  label;
    juce::Label  valueLabel;

    RotarySliderWithLabel(const juce::String& name, const juce::String& suffix)
    {
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(11.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaacc));
        addAndMakeVisible(label);

        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(0xff00bcd4));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c2c44));
        slider.setColour(juce::Slider::thumbColourId,               juce::Colour(0xff00e5ff));
        slider.setVelocityBasedMode(false);
        slider.setMouseDragSensitivity(180);
        addAndMakeVisible(slider);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(10.0f));
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00bcd4));
        addAndMakeVisible(valueLabel);

        slider.onValueChange = [this, suffix]() {
            float val = (float)slider.getValue();
            if      (suffix == "%")   valueLabel.setText(juce::String((int)val) + suffix,     juce::dontSendNotification);
            else if (suffix == " ms") valueLabel.setText(juce::String(val, 1)   + suffix,     juce::dontSendNotification);
            else if (suffix == " dB") valueLabel.setText(juce::String(val, 1)   + suffix,     juce::dontSendNotification);
            else if (suffix == " ct") valueLabel.setText(juce::String((int)val) + suffix,     juce::dontSendNotification);
            else if (suffix == " Hz") valueLabel.setText(juce::String(val, 2)   + suffix,     juce::dontSendNotification);
            else                      valueLabel.setText(juce::String((int)val),               juce::dontSendNotification);
        };
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds(b.removeFromTop(16));
        auto sliderSize = juce::jmin(b.getWidth(), b.getHeight() - 14);
        slider.setBounds(b.withSizeKeepingCentre(sliderSize, sliderSize).reduced(2));
        valueLabel.setBounds(b.removeFromBottom(13));
    }
};

// ─── PitchDisplay ─────────────────────────────────────────────────────────────

struct QPitchAudioProcessorEditor::PitchDisplay : juce::Component {
    float currentPitch  = 0.0f;
    float targetPitch   = 0.0f;
    float correctionCents = 0.0f;
    juce::String currentNote;
    juce::String targetNote;
    bool pendingRepaint = false;

    void setPitches(float detected, float target, float referenceHz)
    {
        if (std::abs(currentPitch - detected) < 0.1f && std::abs(targetPitch - target) < 0.1f)
            return;
        currentPitch = detected;
        targetPitch  = target;
        currentNote  = noteNameFromFreq(detected, referenceHz);
        targetNote   = noteNameFromFreq(target,   referenceHz);
        pendingRepaint = true;
    }

    void setCorrectionCents(float cents)
    {
        if (std::abs(correctionCents - cents) < 0.5f && !pendingRepaint)
            return;
        correctionCents = cents;
        pendingRepaint  = false;
        repaint();
    }

    static juce::String noteNameFromFreq(float freq, float referenceHz)
    {
        if (freq <= 0) return "--";
        float midi = 69.0f + 12.0f * std::log2(freq / referenceHz);
        int   note = (int)std::round(midi);
        static const char* names[] = { "C","C#","D","Eb","E","F","F#","G","G#","A","Bb","B" };
        return names[note % 12] + juce::String(note / 12 - 1);
    }

    void paint(juce::Graphics& g) override
    {
        // Horizontal compact layout for the narrow top strip
        auto bounds = getLocalBounds().reduced(3, 4);

        // Panel background
        g.setColour(juce::Colour(0xff131322));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff2e2e48));
        g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 1.0f);

        // Left block: IN note
        auto inBlock = bounds.removeFromLeft(72).reduced(4, 2);
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xff00bcd4));
        g.drawText("IN", inBlock.removeFromTop(11), juce::Justification::centred);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.setColour(currentPitch > 0 ? juce::Colours::white : juce::Colour(0xff333355));
        g.drawText(currentPitch > 0 ? currentNote : "--", inBlock, juce::Justification::centred);

        // Right block: OUT note
        auto outBlock = bounds.removeFromRight(72).reduced(4, 2);
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.setColour(juce::Colour(0xffff6b6b));
        g.drawText("OUT", outBlock.removeFromTop(11), juce::Justification::centred);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.setColour(targetPitch > 0 ? juce::Colours::white : juce::Colour(0xff333355));
        g.drawText(targetPitch > 0 ? targetNote : "--", outBlock, juce::Justification::centred);

        // Centre: correction meter + cents label
        auto centre = bounds.reduced(6, 3);

        // Cents label above meter
        auto labelRow = centre.removeFromTop(13);
        const juce::String centsStr = (correctionCents >= 0.0f ? "+" : "")
                                      + juce::String(correctionCents, 0) + " ct";
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.setColour(std::abs(correctionCents) > 25.0f ? juce::Colour(0xffff8a00)
                                                       : juce::Colour(0xff00bcd4));
        g.drawText(centsStr, labelRow, juce::Justification::centred);

        // Meter bar
        const float meterH = 5.0f;
        const float meterY = (float)centre.getCentreY() - meterH * 0.5f;
        const float meterX = (float)centre.getX();
        const float meterW = (float)centre.getWidth();
        const float midX   = meterX + meterW * 0.5f;

        g.setColour(juce::Colour(0xff1e1e35));
        g.fillRoundedRectangle(meterX, meterY, meterW, meterH, meterH * 0.5f);

        const float clamped = juce::jlimit(-50.0f, 50.0f, correctionCents);
        const float fillW   = std::abs(clamped) / 50.0f * (meterW * 0.5f);
        const juce::Colour fillCol = std::abs(clamped) > 25.0f ? juce::Colour(0xffff8a00)
                                                                : juce::Colour(0xff00bcd4);
        if (fillW > 0.5f)
        {
            g.setColour(fillCol);
            g.fillRoundedRectangle(clamped >= 0.0f ? midX : midX - fillW,
                                   meterY, fillW, meterH, meterH * 0.5f);
        }

        // Centre tick
        g.setColour(juce::Colour(0xff444466));
        g.fillRect(midX - 0.5f, meterY - 1.0f, 1.0f, meterH + 2.0f);
    }
};

// ─── ScaleKeyboard ────────────────────────────────────────────────────────────

class QPitchAudioProcessorEditor::ScaleKeyboard : public juce::Component {
public:
    explicit ScaleKeyboard(QPitchAudioProcessor& p) : processor(p) {}

    void setKeyAndScale(int newKey, int newScale)
    {
        rootKey   = juce::jlimit(0, 11, newKey);
        scaleType = juce::jlimit(0, ScaleQuantizer::numScales() - 1, newScale);
        repaint();
    }

    void setLivePitches(float detectedHz, float targetHz)
    {
        const float newDetected = hzToMidi(detectedHz);
        const float newTarget   = hzToMidi(targetHz);
        if (std::abs(detectedMidi - newDetected) < 0.02f && std::abs(targetMidi - newTarget) < 0.02f)
            return;
        detectedMidi = newDetected;
        targetMidi   = newTarget;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);

        g.setColour(juce::Colour(0xff111122));
        g.fillRoundedRectangle(bounds, 7.0f);
        g.setColour(juce::Colour(0xff2e2e48));
        g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

        auto labelArea = bounds.removeFromTop(20.0f);
        g.setColour(juce::Colour(0xff888899));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("SCALE KEYBOARD", labelArea, juce::Justification::centredLeft);
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

    static constexpr int numOctaves        = 2;
    static constexpr int whiteKeysPerOctave = 7;
    static constexpr int totalWhiteKeys    = numOctaves * whiteKeysPerOctave;

    int   rootKey   = 0;
    int   scaleType = 2;
    float detectedMidi = -1.0f;
    float targetMidi   = -1.0f;

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

    bool isScaleNote(int absoluteNote) const { return processor.isCustomNoteEnabled(absoluteNote); }
    bool isRoot(int absoluteNote)      const { return (absoluteNote % 12) == rootKey; }

    void drawWhiteKeys(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float keyW = area.getWidth() / (float)totalWhiteKeys;
        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            const int note    = noteForWhiteKey(i);
            auto      r       = juce::Rectangle<float>(area.getX() + keyW * i, area.getY(), keyW, area.getHeight());
            const bool allowed = isScaleNote(note);

            g.setColour(allowed ? juce::Colour(0xffd8fbff) : juce::Colour(0xff4a4a58));
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

            g.setColour(juce::Colour(0xff22222e));
            g.drawRect(r, 1.0f);
        }
    }

    void drawBlackKeys(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float keyW   = area.getWidth() / (float)totalWhiteKeys;
        const float blackW = keyW * 0.58f;
        const float blackH = area.getHeight() * 0.62f;

        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            if (!hasBlackAfterWhite(i % whiteKeysPerOctave)) continue;

            const int   note    = noteForWhiteKey(i) + 1;
            const float centerX = area.getX() + keyW * ((float)i + 1.0f);
            auto        r       = juce::Rectangle<float>(centerX - blackW * 0.5f, area.getY(), blackW, blackH);
            const bool  allowed = isScaleNote(note);

            g.setColour(allowed ? juce::Colour(0xff053b44) : juce::Colour(0xff111118));
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

            g.setColour(juce::Colour(0xff1e1e28));
            g.drawRoundedRectangle(r, 2.5f, 1.0f);
        }
    }

    float xForMidi(juce::Rectangle<float> area, float midi) const
    {
        if (midi < 0.0f) return -1.0f;
        static constexpr int whiteNotes[] = { 0,2,4,5,7,9,11,12,14,16,17,19,21,23 };
        const float wrapped    = std::fmod(midi + 120.0f, 24.0f);
        int         lowerWhite = 0;
        for (int i = 0; i < totalWhiteKeys; ++i)
            if ((float)whiteNotes[i] <= wrapped) lowerWhite = i;
        const float keyW     = area.getWidth() / (float)totalWhiteKeys;
        const int   nextWhite = std::min(lowerWhite + 1, totalWhiteKeys - 1);
        const float span      = std::max(1.0f, (float)(whiteNotes[nextWhite] - whiteNotes[lowerWhite]));
        const float frac      = std::clamp((wrapped - (float)whiteNotes[lowerWhite]) / span, 0.0f, 1.0f);
        return area.getX() + ((float)lowerWhite + frac) * keyW;
    }

    void drawLiveMarkers(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const float inX  = xForMidi(area, detectedMidi);
        const float outX = xForMidi(area, targetMidi);

        if (inX >= 0.0f)
        {
            g.setColour(juce::Colour(0xffff8a00));
            g.fillEllipse(inX - 5.0f, area.getY() + 8.0f, 10.0f, 10.0f);
            g.drawLine(inX, area.getY() + 20.0f, inX, area.getBottom() - 5.0f, 2.0f);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("IN", (int)(inX - 12.0f), (int)(area.getY() + 20.0f), 24, 14, juce::Justification::centred);
        }

        if (outX >= 0.0f)
        {
            g.setColour(juce::Colour(0xffff2bd6));
            g.fillEllipse(outX - 6.0f, area.getBottom() - 19.0f, 12.0f, 12.0f);
            g.drawLine(outX, area.getY() + 5.0f, outX, area.getBottom() - 21.0f, 2.5f);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("OUT", (int)(outX - 16.0f), (int)(area.getBottom() - 34.0f), 32, 14, juce::Justification::centred);
        }
    }

    void drawLegend(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float x = area.getRight() - 260.0f;
        const float y = area.getY() + 4.0f;
        const auto item = [&](juce::Colour colour, const char* text, float width)
        {
            g.setColour(colour);
            g.fillRoundedRectangle(x, y + 3.0f, 12.0f, 8.0f, 2.0f);
            g.setColour(juce::Colour(0xff9090aa));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(text, (int)(x + 16.0f), (int)y, (int)width, 14, juce::Justification::centredLeft);
            x += width + 22.0f;
        };
        item(juce::Colour(0xff00bcd4), "ON",      24.0f);
        item(juce::Colour(0xff4a4a58), "IGNORED", 56.0f);
        item(juce::Colour(0xffff8a00), "ROOT/IN", 52.0f);
        item(juce::Colour(0xffff2bd6), "OUT",     30.0f);
    }

    int noteClassForPoint(juce::Point<float> point) const
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        bounds.removeFromTop(20.0f);
        auto area = bounds.reduced(4.0f, 3.0f);
        if (!area.contains(point)) return -1;

        const float keyW   = area.getWidth() / (float)totalWhiteKeys;
        const float blackW = keyW * 0.58f;
        const float blackH = area.getHeight() * 0.62f;

        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            if (!hasBlackAfterWhite(i % whiteKeysPerOctave)) continue;
            const float centerX = area.getX() + keyW * ((float)i + 1.0f);
            auto r = juce::Rectangle<float>(centerX - blackW * 0.5f, area.getY(), blackW, blackH);
            if (r.contains(point)) return (noteForWhiteKey(i) + 1) % 12;
        }

        const int white = juce::jlimit(0, totalWhiteKeys - 1, (int)((point.x - area.getX()) / keyW));
        return noteForWhiteKey(white) % 12;
    }
};

// ─── Constructor / Destructor ─────────────────────────────────────────────────

QPitchAudioProcessorEditor::QPitchAudioProcessorEditor(QPitchAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    lf = std::make_unique<QPitchLookAndFeel>();
    setLookAndFeel(lf.get());

    auto& vts = processor.getValueTreeState();

    retuneSpeedSlider    = std::make_unique<RotarySliderWithLabel>("Speed",      " ms");
    noteTransitionSlider = std::make_unique<RotarySliderWithLabel>("Transition", " ms");
    correctionAmountSlider = std::make_unique<RotarySliderWithLabel>("Correction", "%");
    toleranceCentsSlider = std::make_unique<RotarySliderWithLabel>("Tolerance",  " ct");
    toleranceTimeSlider  = std::make_unique<RotarySliderWithLabel>("Tol Time",   " ms");
    snappinessSlider     = std::make_unique<RotarySliderWithLabel>("Snappy",     "%");
    tPainSlider          = std::make_unique<RotarySliderWithLabel>("T-Pain",     "%");
    referenceSlider      = std::make_unique<RotarySliderWithLabel>("Reference",  " Hz");
    humanizeSlider       = std::make_unique<RotarySliderWithLabel>("Humanize",   " ct");
    outputGainSlider     = std::make_unique<RotarySliderWithLabel>("Gain",       " dB");

    for (auto* s : { retuneSpeedSlider.get(), noteTransitionSlider.get(),
                     correctionAmountSlider.get(), toleranceCentsSlider.get(),
                     toleranceTimeSlider.get(), snappinessSlider.get(),
                     tPainSlider.get(), referenceSlider.get(),
                     humanizeSlider.get(), outputGainSlider.get() })
        addAndMakeVisible(*s);

    retuneAttach         = std::make_unique<SliderAttach>(vts, "retune_speed",         retuneSpeedSlider->slider);
    noteTransitionAttach = std::make_unique<SliderAttach>(vts, "note_transition",       noteTransitionSlider->slider);
    correctionAttach     = std::make_unique<SliderAttach>(vts, "correction_amount",     correctionAmountSlider->slider);
    toleranceCentsAttach = std::make_unique<SliderAttach>(vts, "tolerance_cents",       toleranceCentsSlider->slider);
    toleranceTimeAttach  = std::make_unique<SliderAttach>(vts, "tolerance_time",        toleranceTimeSlider->slider);
    snappinessAttach     = std::make_unique<SliderAttach>(vts, "snappiness",            snappinessSlider->slider);
    tPainAttach          = std::make_unique<SliderAttach>(vts, "tpain",                 tPainSlider->slider);
    referenceAttach      = std::make_unique<SliderAttach>(vts, "reference_frequency",   referenceSlider->slider);
    humanizeAttach       = std::make_unique<SliderAttach>(vts, "humanize",              humanizeSlider->slider);
    outputGainAttach     = std::make_unique<SliderAttach>(vts, "output_gain",           outputGainSlider->slider);

    keyCombo   = std::make_unique<juce::ComboBox>();
    scaleCombo = std::make_unique<juce::ComboBox>();
    rangeCombo = std::make_unique<juce::ComboBox>();

    for (int i = 0; i < 12; ++i)
        keyCombo->addItem(ScaleQuantizer::getKeyName(i), i + 1);
    keyCombo->setSelectedItemIndex(0);

    for (int i = 0; i < ScaleQuantizer::numScales(); ++i)
        scaleCombo->addItem(ScaleQuantizer::getScaleName(i), i + 1);
    scaleCombo->setSelectedItemIndex(2);

    rangeCombo->addItemList({ "Bass","Baritone","Tenor","Alto","Mezzo Soprano","Soprano","Generic" }, 1);
    rangeCombo->setSelectedItemIndex(6);

    for (auto* c : { keyCombo.get(), scaleCombo.get(), rangeCombo.get() })
    {
        c->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff181828));
        c->setColour(juce::ComboBox::textColourId,       juce::Colours::white);
        c->setColour(juce::ComboBox::outlineColourId,    juce::Colour(0xff3a3a5c));
        c->setColour(juce::ComboBox::arrowColourId,      juce::Colour(0xff00bcd4));
        addAndMakeVisible(*c);
    }

    keyAttach   = std::make_unique<ComboAttach>(vts, "key",   *keyCombo);
    scaleAttach = std::make_unique<ComboAttach>(vts, "scale", *scaleCombo);
    rangeAttach = std::make_unique<ComboAttach>(vts, "range", *rangeCombo);

    correctionToggle = std::make_unique<juce::ToggleButton>("Correction On");
    formantToggle    = std::make_unique<juce::ToggleButton>("Formant: Corrected");

    for (auto* t : { correctionToggle.get(), formantToggle.get() })
    {
        t->setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
        t->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00bcd4));
        addAndMakeVisible(*t);
    }

    correctionOnAttach = std::make_unique<ButtonAttach>(vts, "correction_on", *correctionToggle);
    formantAttach      = std::make_unique<ButtonAttach>(vts, "formant_on",    *formantToggle);

    resetScaleButton = std::make_unique<juce::TextButton>("Reset Notes");
    resetScaleButton->setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff181828));
    resetScaleButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    resetScaleButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00bcd4));
    resetScaleButton->onClick = [this]()
    {
        processor.resetCustomNotesToScale();
        if (scaleKeyboard) scaleKeyboard->repaint();
    };
    addAndMakeVisible(*resetScaleButton);

    pitchDisplay  = std::make_unique<PitchDisplay>();
    scaleKeyboard = std::make_unique<ScaleKeyboard>(processor);
    addAndMakeVisible(*pitchDisplay);
    addAndMakeVisible(*scaleKeyboard);

    auto refreshKeyboard = [this]()
    {
        processor.resetCustomNotesToScale();
        if (scaleKeyboard && keyCombo && scaleCombo)
            scaleKeyboard->setKeyAndScale(keyCombo->getSelectedItemIndex(), scaleCombo->getSelectedItemIndex());
    };
    keyCombo->onChange   = refreshKeyboard;
    scaleCombo->onChange = refreshKeyboard;
    refreshKeyboard();

    setResizable(true, true);
    setResizeLimits(820, 680, 1200, 920);
    setSize(880, 720);
    startTimerHz(24);
}

QPitchAudioProcessorEditor::~QPitchAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

// ─── Timer ────────────────────────────────────────────────────────────────────

void QPitchAudioProcessorEditor::timerCallback()
{
    const float detectedHz = processor.getDebugDetectedHz();
    const float targetHz   = processor.getDebugTargetHz();

    if (pitchDisplay)
    {
        pitchDisplay->setPitches(detectedHz, targetHz, processor.getReferenceFrequency());
        pitchDisplay->setCorrectionCents(processor.getDebugCorrectionCents());
    }

    if (scaleKeyboard)
        scaleKeyboard->setLivePitches(detectedHz, targetHz);
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void QPitchAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().reduced(12);

    // ── Header (44px): title left, pitch display right ──────────────────────
    auto header = b.removeFromTop(44);
    header.removeFromLeft(150);
    pitchDisplay->setBounds(header.reduced(0, 4));

    b.removeFromTop(10);  // gap below header

    // ── Timing section panel (20px label + 108px sliders = 128px) ───────────
    auto timingSection = b.removeFromTop(128);
    timingPanelBounds  = timingSection;
    timingSection.removeFromTop(20);  // space for painted label
    auto sliderRow = timingSection;

    const float sliderW = sliderRow.getWidth() / 5.0f;
    retuneSpeedSlider->setBounds   (sliderRow.removeFromLeft((int)sliderW).reduced(4));
    noteTransitionSlider->setBounds(sliderRow.removeFromLeft((int)sliderW).reduced(4));
    snappinessSlider->setBounds    (sliderRow.removeFromLeft((int)sliderW).reduced(4));
    tPainSlider->setBounds         (sliderRow.removeFromLeft((int)sliderW).reduced(4));
    referenceSlider->setBounds     (sliderRow.reduced(4));

    b.removeFromTop(8);  // gap between sections

    // ── Correction section panel (20px label + 108px sliders = 128px) ───────
    auto correctionSection = b.removeFromTop(128);
    correctionPanelBounds  = correctionSection;
    correctionSection.removeFromTop(20);
    auto sliderRow2 = correctionSection;

    const float sliderW2 = sliderRow2.getWidth() / 5.0f;
    correctionAmountSlider->setBounds(sliderRow2.removeFromLeft((int)sliderW2).reduced(4));
    toleranceCentsSlider->setBounds  (sliderRow2.removeFromLeft((int)sliderW2).reduced(4));
    toleranceTimeSlider->setBounds   (sliderRow2.removeFromLeft((int)sliderW2).reduced(4));
    humanizeSlider->setBounds        (sliderRow2.removeFromLeft((int)sliderW2).reduced(4));
    outputGainSlider->setBounds      (sliderRow2.removeFromLeft((int)sliderW2).reduced(4));

    b.removeFromTop(8);  // gap

    // ── Toggle row (32px) ────────────────────────────────────────────────────
    auto toggleRow = b.removeFromTop(32);
    const int toggleW = toggleRow.getWidth() / 3;
    correctionToggle->setBounds(toggleRow.removeFromLeft(toggleW).reduced(6, 2));
    formantToggle->setBounds   (toggleRow.removeFromLeft(toggleW).reduced(6, 2));

    b.removeFromTop(10);  // gap

    // ── KEY / SCALE combos + Reset (14px painted label + 28px combo) ─────────
    b.removeFromTop(14);  // reserved for "KEY" / "SCALE" painted labels
    auto comboRow = b.removeFromTop(28);
    comboRowBounds = comboRow;

    const int comboW = comboRow.getWidth() / 3;
    keyCombo->setBounds  (comboRow.removeFromLeft(comboW).reduced(2, 0));
    scaleCombo->setBounds(comboRow.removeFromLeft(comboW).reduced(2, 0));
    resetScaleButton->setBounds(comboRow.reduced(2, 0));

    b.removeFromTop(8);  // gap

    // ── Voice range combo (14px painted label + 28px combo) ─────────────────
    b.removeFromTop(14);  // reserved for "VOICE RANGE" painted label
    auto rangeRow = b.removeFromTop(28);
    rangeRowBounds = rangeRow;
    rangeCombo->setBounds(rangeRow.reduced(2, 0));

    b.removeFromTop(8);  // gap

    // ── Scale keyboard (remaining) ────────────────────────────────────────────
    scaleKeyboard->setBounds(b);
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void QPitchAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff0c0c1e));

    const int W = getWidth();

    // ── Header bar ──────────────────────────────────────────────────────────
    juce::Rectangle<float> headerBg(0.0f, 0.0f, (float)W, 56.0f);
    g.setColour(juce::Colour(0xff111122));
    g.fillRect(headerBg);
    g.setColour(juce::Colour(0xff1e1e38));
    g.fillRect(juce::Rectangle<float>(0.0f, 54.0f, (float)W, 2.0f));

    // Plugin name
    g.setColour(juce::Colour(0xff00bcd4));
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    g.drawText("QPitch", 14, 6, 140, 34, juce::Justification::centredLeft);

    // Subtitle + version — both in the left title column, no overlap with pitch display
    g.setColour(juce::Colour(0xff3a3a5c));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("PITCH CORRECTION", 14, 34, 90, 14, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffff8a00));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("v1", 110, 34, 36, 14, juce::Justification::centredLeft);

    // ── Section panels (use bounds saved in resized()) ──────────────────────
    const auto drawPanel = [&](juce::Rectangle<int> r, juce::Colour accent, const char* label)
    {
        auto rf = r.toFloat();
        g.setColour(juce::Colour(0xff111122));
        g.fillRoundedRectangle(rf, 7.0f);
        g.setColour(juce::Colour(0xff1e1e38));
        g.drawRoundedRectangle(rf, 7.0f, 1.0f);
        g.setColour(accent);
        g.fillRoundedRectangle(rf.getX() + 1.0f, rf.getY() + 8.0f, 3.0f, rf.getHeight() - 16.0f, 1.5f);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(label, r.getX() + 10, r.getY() + 3, 220, 16, juce::Justification::centredLeft);
    };

    drawPanel(timingPanelBounds,     juce::Colour(0xff00bcd4), "TIMING & CHARACTER");
    drawPanel(correctionPanelBounds, juce::Colour(0xffff8a00), "CORRECTION & OUTPUT");

    // ── Combo labels (14px reserved above each combo row in resized) ─────────
    const int cw = comboRowBounds.getWidth() / 3;
    // Labels sit in the 14px space immediately above the combo row
    const int keyLabelY   = comboRowBounds.getY() - 14;
    const int rangeLabelY = rangeRowBounds.getY() - 14;

    g.setColour(juce::Colour(0xff555577));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("KEY",         comboRowBounds.getX(),               keyLabelY, cw, 14, juce::Justification::centredLeft);
    g.drawText("SCALE",       comboRowBounds.getX() + cw,          keyLabelY, cw, 14, juce::Justification::centredLeft);
    g.drawText("VOICE RANGE", rangeRowBounds.getX(), rangeLabelY, rangeRowBounds.getWidth(), 14, juce::Justification::centredLeft);
}
