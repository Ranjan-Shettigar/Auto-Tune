#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cmath>

namespace VocalTune
{

static const std::array<const char*, 12> NoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

enum class ScaleType
{
    NaturalMinor = 0,
    Major,
    Chromatic,
    HarmonicMinor,
    MelodicMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    PentatonicMajor,
    PentatonicMinor,
    Blues,
    Custom,
    NumScaleTypes
};

static const std::array<const char*, static_cast<size_t>(ScaleType::NumScaleTypes)> ScaleNames = {
    "Natural Minor",
    "Major",
    "Chromatic",
    "Harmonic Minor",
    "Melodic Minor",
    "Dorian",
    "Phrygian",
    "Lydian",
    "Mixolydian",
    "Pentatonic Major",
    "Pentatonic Minor",
    "Blues",
    "Custom"
};

inline uint16_t getScaleIntervalMask(ScaleType type)
{
    switch (type)
    {
        case ScaleType::NaturalMinor:
            // 0, 2, 3, 5, 7, 8, 10
            return (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);

        case ScaleType::Major:
            // 0, 2, 4, 5, 7, 9, 11
            return (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);

        case ScaleType::Chromatic:
            return 0x0FFF;

        case ScaleType::HarmonicMinor:
            return (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11);

        case ScaleType::MelodicMinor:
            return (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);

        case ScaleType::Dorian:
            return (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10);

        case ScaleType::Phrygian:
            return (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);

        case ScaleType::Lydian:
            return (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11);

        case ScaleType::Mixolydian:
            return (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10);

        case ScaleType::PentatonicMajor:
            return (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9);

        case ScaleType::PentatonicMinor:
            return (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10);

        case ScaleType::Blues:
            return (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10);

        case ScaleType::Custom:
        default:
            return 0x0FFF;
    }
}

inline uint16_t computeActiveNotesMask(int rootNote, ScaleType type)
{
    if (type == ScaleType::Chromatic)
        return 0x0FFF;

    uint16_t intervals = getScaleIntervalMask(type);
    uint16_t mask = 0;
    for (int i = 0; i < 12; ++i)
    {
        if (intervals & (1 << i))
        {
            int note = (rootNote + i) % 12;
            mask |= static_cast<uint16_t>(1 << note);
        }
    }
    return mask;
}

struct ScaleSnapResult
{
    float targetFrequency = 440.0f;
    int targetMidiNote = 69;
    int targetNoteInOctave = 9;
    int targetOctave = 4;
    float centDeviation = 0.0f;
    bool isVoiced = false;
};

inline float frequencyToMidi(float freq, float a4Ref = 440.0f)
{
    if (freq <= 10.0f) return 0.0f;
    return 69.0f + 12.0f * std::log2(freq / a4Ref);
}

inline float midiToFrequency(float midiNote, float a4Ref = 440.0f)
{
    return a4Ref * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

inline ScaleSnapResult snapPitchWithHysteresis(float detectedFreq, uint16_t activeNotesMask, int& currentLockedMidi,
                                              float a4Ref = 440.0f, float hysteresisSemitones = 0.18f)
{
    ScaleSnapResult res;
    if (detectedFreq < 40.0f || detectedFreq > 2000.0f || activeNotesMask == 0)
    {
        res.isVoiced = false;
        return res;
    }

    res.isVoiced = true;
    float exactMidi = frequencyToMidi(detectedFreq, a4Ref);

    // Find nearest active note in scale
    int nearestMidi = static_cast<int>(std::round(exactMidi));
    float minDistance = 100.0f;
    int bestCandidate = nearestMidi;

    for (int offset = -12; offset <= 12; ++offset)
    {
        int cand = nearestMidi + offset;
        int noteInOctave = (cand % 12 + 12) % 12;
        if (activeNotesMask & (1 << noteInOctave))
        {
            float dist = std::abs(exactMidi - static_cast<float>(cand));
            if (dist < minDistance)
            {
                minDistance = dist;
                bestCandidate = cand;
            }
        }
    }

    // Apply Schmitt Trigger Hysteresis to prevent note hunting/flicker
    int selectedMidi = bestCandidate;
    if (currentLockedMidi > 0)
    {
        int lockedNoteInOctave = (currentLockedMidi % 12 + 12) % 12;
        bool isLockedStillActive = (activeNotesMask & (1 << lockedNoteInOctave)) != 0;

        if (isLockedStillActive)
        {
            float distToLocked = std::abs(exactMidi - static_cast<float>(currentLockedMidi));
            float distToCand   = std::abs(exactMidi - static_cast<float>(bestCandidate));

            // Only switch to new candidate if it's strictly closer by at least hysteresis margin
            if (distToCand + hysteresisSemitones < distToLocked)
            {
                selectedMidi = bestCandidate;
                currentLockedMidi = selectedMidi;
            }
            else
            {
                selectedMidi = currentLockedMidi;
            }
        }
        else
        {
            currentLockedMidi = selectedMidi;
        }
    }
    else
    {
        currentLockedMidi = selectedMidi;
    }

    res.targetMidiNote = selectedMidi;
    res.targetNoteInOctave = (selectedMidi % 12 + 12) % 12;
    res.targetOctave = (selectedMidi / 12) - 1;
    res.targetFrequency = midiToFrequency(static_cast<float>(selectedMidi), a4Ref);

    // Cent deviation: positive = sharp, negative = flat
    res.centDeviation = (exactMidi - static_cast<float>(selectedMidi)) * 100.0f;
    res.centDeviation = juce::jlimit(-100.0f, 100.0f, res.centDeviation);

    return res;
}

} // namespace VocalTune
