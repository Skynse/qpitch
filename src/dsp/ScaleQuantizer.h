#pragma once
#include <cmath>
#include <array>
#include <string>
#include <vector>
#include <algorithm>

class ScaleQuantizer {
public:
    enum ScaleType {
        Chromatic = 0,
        Major,
        Minor,
        MajorPentatonic,
        MinorPentatonic,
        Blues,
        Dorian,
        Phrygian,
        Mixolydian,
        WholeTone,
        Diminished,
        NumScales
    };

    ScaleQuantizer();
    ~ScaleQuantizer() = default;

    float hzToMidi(float freqHz) const;
    float midiToHz(float midiNote) const;
    void setReferenceFrequency(float frequencyHz);
    float getReferenceFrequency() const { return referenceFrequencyHz; }
    int quantizeMidiNote(int midiNote, int rootKey, int scaleType) const;
    float quantize(float freqHz, int rootKey, int scaleType) const;
    float getTargetFrequency(float detectedHz, int rootKey, int scaleType, float correctionAmount) const;

    static const std::array<std::array<bool, 12>, NumScales>& getScaleMasks();
    static const char* getScaleName(int scaleType);
    static const char* getKeyName(int rootKey);
    static int numKeys() { return 12; }
    static int numScales() { return NumScales; }

private:
    static constexpr float A4_FREQ = 440.0f;
    static constexpr int A4_MIDI = 69;
    float referenceFrequencyHz = A4_FREQ;
};
