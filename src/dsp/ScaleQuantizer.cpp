#include "ScaleQuantizer.h"

static const std::array<std::array<bool, 12>, ScaleQuantizer::NumScales> scaleMasks = {{
    // Chromatic - all notes
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    // Major: W-W-H-W-W-W-H
    {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1},
    // Minor (Natural): W-H-W-W-H-W-W
    {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0},
    // Major Pentatonic
    {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0},
    // Minor Pentatonic
    {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0},
    // Blues (minor blues): root, b3, 4, b5, 5, b7
    {1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0},
    // Dorian
    {1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0},
    // Phrygian
    {1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0},
    // Mixolydian
    {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0},
    // Whole Tone
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0},
    // Diminished (whole-half): root, W, H, W, H, W, H, W
    {1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1},
}};

ScaleQuantizer::ScaleQuantizer() {}

float ScaleQuantizer::hzToMidi(float freqHz) const
{
    if (freqHz <= 0.0f) return -1.0f;
    return 69.0f + 12.0f * std::log2(freqHz / A4_FREQ);
}

float ScaleQuantizer::midiToHz(float midiNote) const
{
    return A4_FREQ * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

int ScaleQuantizer::quantizeMidiNote(int midiNote, int rootKey, int scaleType) const
{
    if (scaleType == Chromatic)
        return midiNote;

    if (scaleType < 0 || scaleType >= NumScales)
        return midiNote;

    const auto& mask = scaleMasks[scaleType];
    int octave = midiNote / 12;
    int noteInOctave = ((midiNote % 12) - rootKey + 12) % 12;

    int bestNote = noteInOctave;
    int bestDist = 12;

    for (int n = 0; n < 12; ++n)
    {
        if (mask[n])
        {
            int dist = std::abs(n - noteInOctave);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestNote = n;
            }
        }
    }

    int noteClass = (bestNote + rootKey) % 12;
    int result = octave * 12 + noteClass;
    // noteClass may be in the adjacent octave relative to the input; correct for it
    if (result - midiNote > 6)  result -= 12;
    else if (midiNote - result > 6) result += 12;
    return result;
}

float ScaleQuantizer::quantize(float freqHz, int rootKey, int scaleType) const
{
    if (freqHz <= 0.0f) return 0.0f;

    float midiNote = hzToMidi(freqHz);
    int roundedMidi = static_cast<int>(std::round(midiNote));
    int quantizedMidi = quantizeMidiNote(roundedMidi, rootKey, scaleType);

    return midiToHz(static_cast<float>(quantizedMidi));
}

float ScaleQuantizer::getTargetFrequency(float detectedHz, int rootKey, int scaleType, float correctionAmount) const
{
    if (detectedHz <= 0.0f) return 0.0f;
    if (correctionAmount <= 0.0f) return detectedHz;

    float targetHz = quantize(detectedHz, rootKey, scaleType);

    if (correctionAmount >= 1.0f) return targetHz;

    return detectedHz + (targetHz - detectedHz) * correctionAmount;
}

const std::array<std::array<bool, 12>, ScaleQuantizer::NumScales>& ScaleQuantizer::getScaleMasks()
{
    return scaleMasks;
}

const char* ScaleQuantizer::getScaleName(int scaleType)
{
    static const char* names[] = {
        "Chromatic", "Major", "Minor", "Major Pent.", "Minor Pent.",
        "Blues", "Dorian", "Phrygian", "Mixolydian", "Whole Tone", "Diminished"
    };
    if (scaleType >= 0 && scaleType < NumScales)
        return names[scaleType];
    return "Unknown";
}

const char* ScaleQuantizer::getKeyName(int rootKey)
{
    static const char* names[] = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"
    };
    if (rootKey >= 0 && rootKey < 12)
        return names[rootKey];
    return "Unknown";
}
