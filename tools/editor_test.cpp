#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <iostream>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>

static juce::Component *find(juce::Component &root, const juce::String &name) {
  if (root.getName() == name || root.getTitle() == name)
    return &root;
  for (auto *child : root.getChildren())
    if (auto *result = find(*child, name))
      return result;
  return nullptr;
}
static juce::TextButton *button(juce::Component &root,
                                const juce::String &name) {
  for (auto *child : root.getChildren()) {
    if (auto *b = dynamic_cast<juce::TextButton *>(child))
      if (b->getButtonText() == name)
        return b;
    if (auto *result = button(*child, name))
      return result;
  }
  return nullptr;
}
static bool save(juce::Component &editor, const juce::File &path) {
  auto image = editor.createComponentSnapshot(editor.getLocalBounds());
  juce::PNGImageFormat png;
  path.deleteFile();
  juce::FileOutputStream stream(path);
  return stream.openedOk() && png.writeImageToStream(image, stream);
}
int main(int argc, char **argv) {
  juce::ScopedJuceInitialiser_GUI gui;
  QPitchAudioProcessor processor;
  processor.prepareToPlay(48000, 127);
  if (processor.getLatencySamples() <= 0)
    return 1;
  auto &v = processor.getValueTreeState();
  auto set = [&](const char *id, float value) {
    auto *p = v.getParameter(id);
    p->setValueNotifyingHost(p->convertTo0to1(value));
  };
  QPitchAudioProcessorEditor editor(processor);
  const juce::File output(argc > 1 ? argv[1] : "/tmp/qpitch-ui-check");
  output.createDirectory();
  // Render real live feedback from a slightly flat C4, without opening an audio
  // device.
  juce::MidiBuffer previewMidi;
  for (int block = 0; block < 100; ++block) {
    juce::AudioBuffer<float> audio(2, 512);
    for (int i = 0; i < 512; ++i) {
      float sample = .2f * std::sin(2.0 * juce::MathConstants<double>::pi *
                                    261.2 * (block * 512 + i) / 48000.0);
      audio.setSample(0, i, sample);
      audio.setSample(1, i, sample);
    }
    processor.processBlock(audio, previewMidi);
  }
  if (std::abs(processor.getOutputHz() - 261.6256f) > 4.0f)
    return 23; // Verify the marker measures the rendered audio, not the
               // requested target.
  editor.timerCallback();
  if (!save(editor, output.getChildFile("main.png")))
    return 2;
  auto *advanced = button(editor, "+ Detailed tuning");
  if (!advanced)
    return 3;
  advanced->onClick();
  if (editor.getHeight() != 660 ||
      !save(editor, output.getChildFile("detailed.png")))
    return 4;
  advanced
      ->onClick(); // Return to the piano without changing the window height.
  if (editor.getHeight() != 660)
    return 22;
  auto *piano = find(editor, "Target notes piano");
  if (!piano)
    return 17;
  auto clickKey = [&](juce::Point<float> position) {
    auto now = juce::Time::getCurrentTime();
    juce::MouseEvent event(
        juce::Desktop::getInstance().getMainMouseSource(), position,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0,
        0, piano, piano, now, position, now, 1, false);
    piano->mouseDown(event);
  };
  clickKey({120, 20}); // C# overlays the C/D white keys.
  if (!processor.isCustomNoteEnabled(1))
    return 18;
  clickKey({50, 70}); // The bottom of C must select the white key.
  if (processor.isCustomNoteEnabled(0))
    return 19;
  piano->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
  if (!processor.isCustomNoteEnabled(0))
    return 20;
  piano->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
  piano->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
  if (processor.isCustomNoteEnabled(1))
    return 21;
  auto *reset = button(editor, "Reset notes");
  if (!reset)
    return 5;
  processor.setCustomNoteEnabled(2, false);
  reset->onClick();
  if (!processor.isCustomNoteEnabled(2))
    return 6;
  auto *speed = dynamic_cast<juce::Slider *>(find(editor, "Retune speed"));
  if (!speed)
    return 7;
  speed->setValue(72, juce::sendNotificationSync);
  if (std::abs(v.getRawParameterValue("retune_speed")->load() - 72) > .1)
    return 8;
  set("retune_speed", 15);
  if (speed->getTextFromValue(15) != "15.0 ms")
    return 15;
  if (std::abs(speed->getValue() - 15) > .1)
    return 9;
  set("key", 2);
  set("scale", 0);
  processor.setCustomNoteEnabled(1, false);
  juce::MemoryBlock state;
  processor.getStateInformation(state);
  QPitchAudioProcessor restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  if (restored.isCustomNoteEnabled(1) ||
      restored.getValueTreeState().getRawParameterValue("key")->load() != 2)
    return 10;
  // Correction off must retain the declared latency and exact dry audio.
  set("correction_on", 0);
  int latency = processor.getLatencySamples(), total = latency + 6000;
  juce::MidiBuffer midi;
  int offset = 0;
  float peak = 0;
  int peakIndex = -1;
  while (offset < total) {
    int n = std::min(127, total - offset);
    juce::AudioBuffer<float> audio(2, n);
    audio.clear();
    if (offset == 0)
      audio.setSample(0, 0, .5f);
    processor.processBlock(audio, midi);
    for (int i = 0; i < n; ++i) {
      float sample = audio.getSample(0, i);
      if (std::abs(sample) > peak) {
        peak = std::abs(sample);
        peakIndex = offset + i;
      }
      if (!std::isfinite(sample))
        return 11;
    }
    offset += n;
  }
  if (peakIndex != latency || std::abs(peak - .5f) > 1.e-5)
    return 12;
  editor.timerCallback();
  if (!save(editor, output.getChildFile("off.png")))
    return 13;
  editor.setSize(1320, 990);
  if (!save(editor, output.getChildFile("scaled.png")))
    return 16;
  // Variable host blocks, formant automation, and bypass must remain finite.
  set("correction_on", 1);
  for (int block : {1, 64, 127, 512, 1023}) {
    set("formant_on", block % 2);
    juce::AudioBuffer<float> audio(2, block);
    for (int i = 0; i < block; ++i) {
      float value = .2f * std::sin(i * .029f);
      audio.setSample(0, i, value);
      audio.setSample(1, i, value);
    }
    processor.processBlock(audio, midi);
    processor.processBlockBypassed(audio, midi);
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < block; ++i)
        if (!std::isfinite(audio.getSample(ch, i)))
          return 14;
  }
  // A duplicated mono vocal must never acquire a left/right delay or phase
  // offset.
  for (bool monoInput : {false, true}) {
    QPitchAudioProcessor monoProcessor;
    if (monoInput) {
      auto layout = monoProcessor.getBusesLayout();
      layout.inputBuses.set(0, juce::AudioChannelSet::mono());
      layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
      if (!monoProcessor.setBusesLayout(layout))
        return 24;
    }
    monoProcessor.prepareToPlay(48000, 512);
    auto &state = monoProcessor.getValueTreeState();
    auto change = [&](const char *id, float value) {
      auto *parameter = state.getParameter(id);
      parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    int time = 0;
    for (int block = 0; block < 180; ++block) {
      change("formant_on", (block / 30) % 2);
      change("correction_on", block < 120 ? 1.f : 0.f);
      change("correction_amount", block < 60 ? 100.f : 45.f);
      const int n = block % 2 ? 127 : 512;
      juce::AudioBuffer<float> audio(2, n);
      for (int i = 0; i < n; ++i) {
        double phase = (time + i) * 2.0 * juce::MathConstants<double>::pi *
                       227.0 / 48000.0;
        float value = .2f * std::sin(phase) + .08f * std::sin(2 * phase) +
                      .04f * std::sin(3 * phase);
        audio.setSample(0, i, value);
        audio.setSample(1, i, monoInput ? 0.f : value);
      }
      monoProcessor.processBlock(audio, midi);
      for (int i = 0; i < n; ++i)
        if (std::abs(audio.getSample(0, i) - audio.getSample(1, i)) > 1.e-6f)
          return 25;
      time += n;
    }
  }
  std::cout << "PASS mono vocal has identical L/R output through correction, "
               "formants and bypass\n";
  std::cout << "PASS UI attachments, disclosure, reset, state restore, "
               "latency, variable blocks and host "
               "bypass\n";
  return 0;
}
