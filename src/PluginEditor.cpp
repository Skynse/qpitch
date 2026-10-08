#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/ScaleQuantizer.h"
#include <cmath>

namespace {
const juce::Colour background(0xff15191b), panel(0xff1c2225), field(0xff242c30),
    line(0xff354045), text(0xfff1f3ee), muted(0xffacb7b9), lime(0xffc7f278),
    cyan(0xff78d6dc), amber(0xfff3bc69);
constexpr int designWidth = 880, compactHeight = 660;
juce::Font font(float size, bool bold = false) {
  return juce::Font(
      juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
void caption(juce::Graphics &g, const juce::String &value, int x, int y, int w,
             int h = 20, float size = 12, juce::Colour colour = muted,
             bool bold = false) {
  g.setColour(colour);
  g.setFont(font(size, bold));
  g.drawText(value, x, y, w, h, juce::Justification::centredLeft);
}
} // namespace

class QPitchLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  QPitchLookAndFeel() {
    setColour(juce::PopupMenu::backgroundColourId, field);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, lime);
    setColour(juce::PopupMenu::highlightedTextColourId, background);
    setColour(juce::TextEditor::backgroundColourId, field);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::highlightColourId, lime.withAlpha(.3f));
    setColour(juce::TextEditor::focusedOutlineColourId, lime);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, field);
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
  }
  void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h,
                        float value, float start, float end,
                        juce::Slider &slider) override {
    const float r = juce::jmin(w, h) * .5f - 6, cx = x + w * .5f,
                cy = y + h * .5f;
    const float angle = start + value * (end - start);
    juce::Path track, active;
    track.addCentredArc(cx, cy, r, r, 0, start, end, true);
    active.addCentredArc(cx, cy, r, r, 0, start, angle, true);
    const auto stroke = juce::PathStrokeType(5, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded);
    g.setColour(line);
    g.strokePath(track, stroke);
    g.setColour(slider.isEnabled() ? lime : muted.withAlpha(.4f));
    g.strokePath(active, stroke);
    g.setColour(field);
    g.fillEllipse(cx - r + 8, cy - r + 8, (r - 8) * 2, (r - 8) * 2);
    g.setColour(slider.isEnabled() ? lime : muted);
    g.drawLine(cx + std::sin(angle) * r * .38f, cy - std::cos(angle) * r * .38f,
               cx + std::sin(angle) * r * .69f, cy - std::cos(angle) * r * .69f,
               3);
    if (slider.hasKeyboardFocus(false)) {
      g.setColour(lime);
      g.drawEllipse(cx - r - 4, cy - r - 4, (r + 4) * 2, (r + 4) * 2, 1);
    }
  }
  void drawLinearSlider(juce::Graphics &g, int x, int y, int w, int h,
                        float pos, float, float,
                        juce::Slider::SliderStyle style,
                        juce::Slider &s) override {
    if (style == juce::Slider::LinearBar) {
      g.setColour(field);
      g.fillRoundedRectangle(s.getLocalBounds().toFloat(), 6);
      if (s.hasKeyboardFocus(true)) {
        g.setColour(lime);
        g.drawRoundedRectangle(s.getLocalBounds().toFloat().reduced(.5f), 6, 1);
      }
      return;
    }
    float yy = y + h * .5f;
    g.setColour(line);
    g.drawLine((float)x, yy, (float)(x + w), yy, 3);
    g.setColour(s.isEnabled() ? lime : muted);
    g.fillEllipse(pos - 6, yy - 6, 12, 12);
  }
  juce::Label *createSliderTextBox(juce::Slider &s) override {
    auto *label = juce::LookAndFeel_V4::createSliderTextBox(s);
    label->setFont(
        font(s.getSliderStyle() == juce::Slider::RotaryVerticalDrag ? 22 : 17));
    return label;
  }
  void drawComboBox(juce::Graphics &g, int w, int h, bool, int, int, int, int,
                    juce::ComboBox &box) override {
    g.setColour(field);
    g.fillRoundedRectangle(0, 0, (float)w, (float)h, 6);
    if (box.hasKeyboardFocus(false)) {
      g.setColour(lime);
      g.drawRoundedRectangle(.5f, .5f, w - 1.f, h - 1.f, 6, 1);
    }
    juce::Path p;
    p.startNewSubPath(w - 24.f, h * .45f);
    p.lineTo(w - 19.f, h * .57f);
    p.lineTo(w - 14.f, h * .45f);
    g.setColour(muted);
    g.strokePath(p, juce::PathStrokeType(1.5f));
  }
  juce::Font getComboBoxFont(juce::ComboBox &) override { return font(16); }
  void positionComboBoxText(juce::ComboBox &box, juce::Label &l) override {
    l.setBounds(12, 0, box.getWidth() - 42, box.getHeight());
    l.setFont(getComboBoxFont(box));
  }
  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &b, bool hover,
                        bool) override {
    auto r = b.getLocalBounds().toFloat().reduced(1);
    bool on = b.getToggleState();
    g.setColour(on ? lime : field);
    g.fillRoundedRectangle(r, 7);
    g.setColour(on ? background : muted);
    g.fillEllipse(12, r.getCentreY() - 5, 10, 10);
    g.setFont(font(14, true));
    g.drawText(b.getButtonText() + (on ? " on" : " off"), 32, 0,
               b.getWidth() - 38, b.getHeight(),
               juce::Justification::centredLeft);
    if (hover || b.hasKeyboardFocus(false)) {
      g.setColour(lime);
      g.drawRoundedRectangle(r, 7, 1);
    }
  }
  void drawButtonBackground(juce::Graphics &g, juce::Button &b,
                            const juce::Colour &, bool hover,
                            bool down) override {
    auto r = b.getLocalBounds().toFloat().reduced(.5f);
    g.setColour(down ? line : field);
    g.fillRoundedRectangle(r, 6);
    if (hover || b.hasKeyboardFocus(false)) {
      g.setColour(lime);
      g.drawRoundedRectangle(r, 6, 1);
    }
  }
  void drawButtonText(juce::Graphics &g, juce::TextButton &b, bool,
                      bool) override {
    g.setFont(font(14));
    g.setColour(text);
    g.drawText(b.getButtonText(), b.getLocalBounds().reduced(10),
               juce::Justification::centred);
  }
};

class QPitchAudioProcessorEditor::RotarySliderWithLabel final
    : public juce::Component {
public:
  juce::Slider slider;
  juce::Label label;
  juce::String hint;
  bool fieldOnly = false, horizontal = false;
  RotarySliderWithLabel(const juce::String &name, const juce::String &suffix) {
    label.setText(name, juce::dontSendNotification);
    label.setFont(font(16, true));
    label.setColour(juce::Label::textColourId, text);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);
    slider.setName(name);
    slider.setTitle(name);
    slider.setTextValueSuffix(suffix);
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 150, 32);
    slider.setNumDecimalPlacesToDisplay(suffix == " Hz" ? 2
                                        : suffix == "%" ? 0
                                                        : 1);
    addAndMakeVisible(slider);
  }
  void asField() {
    fieldOnly = true;
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 240, 42);
  }
  void asHorizontal() {
    horizontal = true;
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 164, 36);
  }
  void resized() override {
    auto b = getLocalBounds();
    if (fieldOnly) {
      label.setVisible(false);
      slider.setBounds(b);
      return;
    }
    label.setBounds(b.removeFromTop(28));
    b.removeFromBottom(horizontal ? 0 : 26);
    slider.setBounds(b.reduced(horizontal ? 0 : 22, 0));
  }
  void paint(juce::Graphics &g) override {
    if (!fieldOnly && !horizontal)
      caption(g, hint, 0, getHeight() - 24, getWidth(), 20, 11);
  }
};

class QPitchAudioProcessorEditor::PitchDisplay final : public juce::Component {
public:
  float detected = 0, target = 0, reference = 440;
  bool active = true;
  static juce::String noteName(float hz, float ref) {
    if (hz <= 0 || !std::isfinite(hz))
      return "--";
    int midi = juce::roundToInt(69 + 12 * std::log2(hz / ref));
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    return juce::String(names[(midi % 12 + 12) % 12]) +
           juce::String(midi / 12 - 1);
  }
  float output = 0;
  void paint(juce::Graphics &g) override {
    g.setColour(juce::Colour(0xff111618));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8);
    caption(g, "INPUT", 20, 10, 140, 20, 11, cyan);
    caption(g, noteName(detected, reference), 20, 35, 180, 40, 30, cyan, true);
    caption(g, "MEASURED OUTPUT", 240, 10, 200, 20, 11, lime);
    caption(g, noteName(output, reference), 240, 35, 180, 40, 30, lime, true);
    caption(g,
            active ? "Follow the moving dots on the piano below"
                   : "Correction off",
            460, 20, 350, 22, 12);
    caption(g, "Cyan: input    Lime: output    Ring: target", 460, 48, 350, 22,
            12);
  }
};

class QPitchAudioProcessorEditor::ScaleKeyboard final : public juce::Component {
public:
  explicit ScaleKeyboard(QPitchAudioProcessor &p) : processor(p) {
    setWantsKeyboardFocus(true);
    setTitle("Target notes piano");
    setDescription("Click a piano key to allow or exclude a note. Arrow keys "
                   "select a note; Space toggles it.");
  }
  void paint(juce::Graphics &g) override {
    auto area = getLocalBounds().toFloat().reduced(4);
    g.setColour(juce::Colour(0xff0b1012));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6);
    float w = area.getWidth() / 7;
    int root =
        (int)processor.getValueTreeState().getRawParameterValue("key")->load();
    for (int i = 0; i < 7; ++i) {
      int n = whites[i];
      auto r = juce::Rectangle<float>(area.getX() + i * w, area.getY(), w - 1,
                                      area.getHeight());
      drawKey(g, r, n, false, root);
    }
    for (int i = 0; i < 6; ++i)
      if (i != 2) {
        int n = whites[i] + 1;
        auto r = juce::Rectangle<float>(area.getX() + (i + 1) * w - w * .29f,
                                        area.getY(), w * .58f,
                                        area.getHeight() * .62f);
        drawKey(g, r, n, true, root);
      }
    drawPitchMarkers(g, area, w);
  }
  void drawPitchMarkers(juce::Graphics &g, juce::Rectangle<float> area,
                        float w) {
    const float reference = processor.getValueTreeState()
                                .getRawParameterValue("reference_frequency")
                                ->load();
    // Map each semitone to the centre of its actual white or black piano key.
    const float centres[] = {.5f,  1.f, 1.5f, 2.f, 2.5f, 3.5f, 4.f,
                             4.5f, 5.f, 5.5f, 6.f, 6.5f, 7.5f};
    auto marker = [&](float hz, float y, juce::Colour colour, bool ring) {
      if (hz <= 0 || !std::isfinite(hz))
        return;
      const float midi = 69.f + 12.f * std::log2(hz / reference);
      float pc = std::fmod(midi, 12.f);
      if (pc < 0)
        pc += 12.f;
      const int n = (int)std::floor(pc);
      float position = centres[n] + (centres[n + 1] - centres[n]) * (pc - n);
      if (position > 7.f)
        position -= 7.f;
      const float x = area.getX() + position * w;
      g.setColour(background);
      g.fillEllipse(x - 8, y - 8, 16, 16);
      g.setColour(colour);
      if (ring)
        g.drawEllipse(x - 7, y - 7, 14, 14, 2);
      else
        g.fillEllipse(x - 5, y - 5, 10, 10);
    };
    marker(processor.getDetectedHz(), area.getY() + 12, cyan, false);
    marker(processor.getTargetHz(), area.getY() + 31, lime, true);
    marker(processor.getOutputHz(), area.getY() + 31, lime, false);
  }
  void mouseDown(const juce::MouseEvent &e) override {
    auto area = getLocalBounds().toFloat().reduced(4);
    if (!area.contains(e.position))
      return;
    float w = area.getWidth() / 7;
    int n = -1;
    for (int i = 0; i < 6; ++i)
      if (i != 2 &&
          juce::Rectangle<float>(area.getX() + (i + 1) * w - w * .29f,
                                 area.getY(), w * .58f, area.getHeight() * .62f)
              .contains(e.position))
        n = whites[i] + 1;
    if (n < 0)
      n = whites[juce::jlimit(0, 6, (int)((e.position.x - area.getX()) / w))];
    focusedNote = n;
    processor.setCustomNoteEnabled(n, !processor.isCustomNoteEnabled(n));
    repaint();
  }
  bool keyPressed(const juce::KeyPress &k) override {
    if (k == juce::KeyPress::leftKey)
      focusedNote = (focusedNote + 11) % 12;
    else if (k == juce::KeyPress::rightKey)
      focusedNote = (focusedNote + 1) % 12;
    else if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey)
      processor.setCustomNoteEnabled(
          focusedNote, !processor.isCustomNoteEnabled(focusedNote));
    else
      return false;
    repaint();
    return true;
  }
  void focusGained(FocusChangeType) override { repaint(); }
  void focusLost(FocusChangeType) override { repaint(); }

private:
  QPitchAudioProcessor &processor;
  static constexpr int whites[] = {0, 2, 4, 5, 7, 9, 11};
  int focusedNote = 0;
  void drawKey(juce::Graphics &g, juce::Rectangle<float> r, int n, bool black,
               int root) {
    bool allowed = processor.isCustomNoteEnabled(n);
    g.setColour(black     ? juce::Colour(0xff11181b)
                : allowed ? juce::Colour(0xffe9eee8)
                          : juce::Colour(0xff697277));
    g.fillRoundedRectangle(r, 3);
    if (black) {
      g.setColour(line);
      g.drawRoundedRectangle(r, 3, 1);
    }
    if (allowed || n == root) {
      g.setColour(n == root ? amber : lime);
      g.fillRoundedRectangle(r.withY(r.getBottom() - 8).withHeight(8), 2);
    }
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    g.setFont(font(black ? 12 : 17, true));
    g.setColour(black ? muted : allowed ? field : text);
    g.drawText(names[n], r.withY(r.getBottom() - 35).withHeight(24),
               juce::Justification::centred);
    if (hasKeyboardFocus(true) && focusedNote == n) {
      g.setColour(cyan);
      g.drawRoundedRectangle(r.reduced(2), 3, 2);
    }
  }
};

QPitchAudioProcessorEditor::QPitchAudioProcessorEditor(QPitchAudioProcessor &p)
    : AudioProcessorEditor(p), processor(p) {
  lf = std::make_unique<QPitchLookAndFeel>();
  setLookAndFeel(lf.get());
  auto &vts = p.getValueTreeState();
  auto make = [this](auto &ptr, const char *name, const char *suffix) {
    ptr = std::make_unique<RotarySliderWithLabel>(name, suffix);
    addAndMakeVisible(*ptr);
    ptr->slider.lookAndFeelChanged();
  };
  make(retuneSpeedSlider, "Retune speed", " ms");
  make(correctionAmountSlider, "Correction", "%");
  make(noteTransitionSlider, "Transition", " ms");
  make(humanizeSlider, "Humanize", " ct");
  humanizeSlider->asHorizontal();
  make(referenceSlider, "Reference", " Hz");
  referenceSlider->asField();
  make(outputGainSlider, "Output", " dB");
  outputGainSlider->asField();
  make(snappinessSlider, "Snappiness", "%");
  make(tPainSlider, "T-Pain", "%");
  make(toleranceCentsSlider, "Tolerance", " ct");
  make(toleranceTimeSlider, "Tolerance time", " ms");
  for (auto *s : {snappinessSlider.get(), tPainSlider.get(),
                  toleranceCentsSlider.get(), toleranceTimeSlider.get()})
    s->asField();
  retuneSpeedSlider->hint = "Fast to gradual";
  correctionAmountSlider->hint = "Amount of pitch correction";
  noteTransitionSlider->hint = "Time between target notes";
  auto attach = [&](auto &a, const char *id, auto &s) {
    a = std::make_unique<SliderAttach>(vts, id, s->slider);
    // Parameter formatters already include units.
    s->slider.setTextValueSuffix({});
    s->slider.setDoubleClickReturnValue(
        true, vts.getParameter(id)->convertFrom0to1(
                  vts.getParameter(id)->getDefaultValue()));
  };
  attach(retuneAttach, "retune_speed", retuneSpeedSlider);
  attach(correctionAttach, "correction_amount", correctionAmountSlider);
  attach(noteTransitionAttach, "note_transition", noteTransitionSlider);
  attach(humanizeAttach, "humanize", humanizeSlider);
  attach(referenceAttach, "reference_frequency", referenceSlider);
  attach(outputGainAttach, "output_gain", outputGainSlider);
  attach(snappinessAttach, "snappiness", snappinessSlider);
  attach(tPainAttach, "tpain", tPainSlider);
  attach(toleranceCentsAttach, "tolerance_cents", toleranceCentsSlider);
  attach(toleranceTimeAttach, "tolerance_time", toleranceTimeSlider);
  retuneSpeedSlider->slider.setSkewFactorFromMidPoint(100);
  noteTransitionSlider->slider.setSkewFactorFromMidPoint(150);
  for (auto *s :
       {retuneSpeedSlider.get(), correctionAmountSlider.get(),
        noteTransitionSlider.get(), humanizeSlider.get(), referenceSlider.get(),
        outputGainSlider.get(), snappinessSlider.get(), tPainSlider.get(),
        toleranceCentsSlider.get(), toleranceTimeSlider.get()})
    s->slider.setTooltip("Drag to adjust; click the value to type. "
                         "Double-click to restore the default.");
  keyCombo = std::make_unique<juce::ComboBox>();
  scaleCombo = std::make_unique<juce::ComboBox>();
  rangeCombo = std::make_unique<juce::ComboBox>();
  for (int i = 0; i < 12; ++i)
    keyCombo->addItem(ScaleQuantizer::getKeyName(i), i + 1);
  for (int i = 0; i < ScaleQuantizer::numScales(); ++i)
    scaleCombo->addItem(ScaleQuantizer::getScaleName(i), i + 1);
  rangeCombo->addItemList({"Bass", "Baritone", "Tenor", "Alto", "Mezzo Soprano",
                           "Soprano", "Generic"},
                          1);
  for (auto *c : {keyCombo.get(), scaleCombo.get(), rangeCombo.get()})
    addAndMakeVisible(*c);
  keyCombo->setTitle("Key");
  scaleCombo->setTitle("Scale");
  rangeCombo->setTitle("Voice range");
  keyAttach = std::make_unique<ComboAttach>(vts, "key", *keyCombo);
  scaleAttach = std::make_unique<ComboAttach>(vts, "scale", *scaleCombo);
  rangeAttach = std::make_unique<ComboAttach>(vts, "range", *rangeCombo);
  correctionToggle = std::make_unique<juce::ToggleButton>("Correction");
  formantToggle = std::make_unique<juce::ToggleButton>("Formants");
  addAndMakeVisible(*correctionToggle);
  addAndMakeVisible(*formantToggle);
  correctionOnAttach =
      std::make_unique<ButtonAttach>(vts, "correction_on", *correctionToggle);
  formantAttach =
      std::make_unique<ButtonAttach>(vts, "formant_on", *formantToggle);
  resetScaleButton = std::make_unique<juce::TextButton>("Reset notes");
  addAndMakeVisible(*resetScaleButton);
  resetScaleButton->onClick = [this] {
    processor.resetCustomNotesToScale();
    scaleKeyboard->repaint();
  };
  advancedButton = std::make_unique<juce::TextButton>("+ Detailed tuning");
  addAndMakeVisible(*advancedButton);
  advancedButton->onClick = [this] {
    advancedOpen = !advancedOpen;
    advancedButton->setButtonText(advancedOpen ? "Show piano"
                                               : "+ Detailed tuning");
    updateSize();
  };
  pitchDisplay = std::make_unique<PitchDisplay>();
  scaleKeyboard = std::make_unique<ScaleKeyboard>(p);
  addAndMakeVisible(*pitchDisplay);
  addAndMakeVisible(*scaleKeyboard);
  tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 700);
  setOpaque(true);
  setResizable(true, false);
  updateSize();
  startTimerHz(24);
}
QPitchAudioProcessorEditor::~QPitchAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}
void QPitchAudioProcessorEditor::updateSize() {
  const double scale = juce::jlimit(
      .9, 1.5, getWidth() > 0 ? getWidth() / (double)designWidth : 1.0);
  setResizeLimits(792, 594, 1320, 990);
  getConstrainer()->setFixedAspectRatio(designWidth / (double)compactHeight);
  setSize(juce::roundToInt(designWidth * scale),
          juce::roundToInt(compactHeight * scale));
  resized();
  repaint();
}
void QPitchAudioProcessorEditor::timerCallback() {
  auto &v = processor.getValueTreeState();
  bool active = v.getRawParameterValue("correction_on")->load() > .5f;
  bool anyNote = false;
  for (int i = 0; i < 12; ++i)
    anyNote = anyNote || processor.isCustomNoteEnabled(i);
  pitchDisplay->detected = processor.getDetectedHz();
  pitchDisplay->target = processor.getTargetHz();
  pitchDisplay->output = processor.getOutputHz();
  pitchDisplay->reference =
      v.getRawParameterValue("reference_frequency")->load();
  pitchDisplay->active = active && anyNote;
  pitchDisplay->repaint();
  scaleKeyboard->repaint();
  for (auto *s : {retuneSpeedSlider.get(), correctionAmountSlider.get(),
                  noteTransitionSlider.get(), humanizeSlider.get(),
                  snappinessSlider.get(), tPainSlider.get(),
                  toleranceCentsSlider.get(), toleranceTimeSlider.get()})
    s->setEnabled(active);
  formantToggle->setEnabled(active);
}
void QPitchAudioProcessorEditor::resized() {
  float scale = getWidth() / (float)designWidth;
  auto put = [scale](juce::Component *c, int x, int y, int w, int h) {
    c->setTransform(juce::AffineTransform());
    c->setBounds(x, y, w, h);
    c->setTransform(juce::AffineTransform::scale(scale));
  };
  put(correctionToggle.get(), 683, 16, 169, 36);
  put(keyCombo.get(), 28, 96, 112, 36);
  put(scaleCombo.get(), 152, 96, 204, 36);
  put(rangeCombo.get(), 368, 96, 220, 36);
  put(referenceSlider.get(), 612, 96, 240, 36);
  put(pitchDisplay.get(), 28, 144, 824, 100);
  put(retuneSpeedSlider.get(), 28, 272, 180, 172);
  put(correctionAmountSlider.get(), 230, 272, 180, 172);
  put(noteTransitionSlider.get(), 432, 272, 180, 172);
  put(humanizeSlider.get(), 672, 300, 164, 82);
  put(formantToggle.get(), 666, 398, 180, 36);
  put(resetScaleButton.get(), 728, 454, 124, 30);
  put(scaleKeyboard.get(), 28, 500, 824, 84);
  put(advancedButton.get(), 28, 620, 190, 32);
  put(outputGainSlider.get(), 710, 620, 142, 32);
  scaleKeyboard->setVisible(!advancedOpen);
  resetScaleButton->setVisible(!advancedOpen);
  int x = 44;
  for (auto *s : {snappinessSlider.get(), tPainSlider.get(),
                  toleranceCentsSlider.get(), toleranceTimeSlider.get()}) {
    s->setVisible(advancedOpen);
    put(s, x, 490, 180, 36);
    x += 204;
  }
}
void QPitchAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(background);
  g.addTransform(juce::AffineTransform::scale(getWidth() / (float)designWidth));
  caption(g, "QPitch", 28, 9, 200, 34, 26, text, true);
  caption(g, "VOCAL PITCH CORRECTION  /  2.0", 29, 43, 360, 14, 10);
  g.setColour(line);
  g.fillRect(0, 64, 880, 1);
  caption(g, "KEY", 28, 74, 112, 18, 11);
  caption(g, "SCALE", 152, 74, 204, 18, 11);
  caption(g, "VOICE RANGE", 368, 74, 220, 18, 11);
  caption(g, "REFERENCE / A4", 612, 74, 240, 18, 11);
  caption(g, "CORRECTION", 28, 250, 200, 18, 11);
  g.setColour(panel);
  g.fillRoundedRectangle(656, 266, 196, 178, 8);
  caption(g, "VOICE CHARACTER", 672, 276, 170, 18, 11);
  if (!advancedOpen) {
    caption(g, "LIVE PITCH / TARGET NOTES", 28, 451, 220, 20, 11);
    caption(g, "Click a key to allow or exclude its note", 28, 474, 430, 18,
            12);
    auto legend = [&](const char *label, int x, juce::Colour c) {
      g.setColour(c);
      g.fillRect(x, 461, 5, 5);
      caption(g, label, x + 11, 451, 92, 20, 11);
    };
    legend("Allowed", 466, lime);
    legend("Excluded", 567, muted);
    legend("Root", 670, amber);
    int allowed = 0;
    for (int i = 0; i < 12; ++i)
      if (processor.isCustomNoteEnabled(i))
        ++allowed;
    caption(
        g,
        allowed > 0
            ? juce::String(allowed) +
                  " allowed notes / Reset restores the selected key and scale"
            : "No allowed notes / choose a piano key to resume correction",
        28, 589, 700, 16, 11);
  } else {
    g.setColour(panel);
    g.fillRoundedRectangle(28, 451, 824, 154, 8);
    int x = 44;
    for (auto *label :
         {"Snappiness", "T-Pain", "Tolerance", "Tolerance time"}) {
      caption(g, label, x, 465, 180, 18, 13);
      x += 204;
    }
    caption(g,
            "Snappiness sharpens correction. Tolerance keeps small deviations "
            "untouched.",
            44, 540, 765, 20, 12);
    caption(g,
            "T-Pain increases the robotic effect. Humanize is reduced at "
            "stronger settings.",
            44, 566, 765, 20, 12);
  }
  g.setColour(line);
  g.fillRect(28, 612, 824, 1);
  caption(g, "Output", 642, 626, 62, 20, 13);
}
