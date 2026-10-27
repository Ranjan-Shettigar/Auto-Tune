#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PitchDetector.h"
#include "PitchShifter.h"
#include "FormantShifter.h"
#include "VibratoGenerator.h"
#include "../Common/ScaleDefinition.h"
#include <atomic>

namespace VocalTune
{

struct VisualizerState
{
    float centDeviation = 0.0f;
    int noteInOctave = 3;  // e.g. D#
    int octave = 3;        // e.g. 3 -> "D# 3"
    bool isVoiced = false;
    float currentPitchHz = 0.0f;
    float targetPitchHz = 0.0f;
    uint16_t activeScaleMask = 0;
};

class AutoTuneEngine
{
public:
    AutoTuneEngine();
    ~AutoTuneEngine() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setLowLatencyMode(bool lowLatency);
    int getLatencySamples() const { return pitchShifter.getLatencySamples(); }

    void process(juce::AudioBuffer<float>& buffer,
                 float correctionPercent,
                 float retuneSpeedMs,
                 float flexTunePercent,
                 float vibratoPercent,
                 float formantShiftSemitones,
                 float harmonizeMix,
                 int harmonyIntervalIndex,
                 int rootNote,
                 ScaleType scaleType,
                 float tuningRef,
                 int vocalRangeIndex,
                 int trackingSensIndex,
                 bool followScale,
                 uint16_t customMask);

    VisualizerState getVisualizerState() const;

private:
    double currentSampleRate = 48000.0;
    int numChannels = 2;
    bool isLowLatency = true;

    PitchDetector pitchDetector;
    PitchShifter pitchShifter;
    PitchShifter harmonyShifter;
    FormantShifter formantShifter;
    VibratoGenerator vibratoGenerator;

    juce::AudioBuffer<float> monoAnalysisBuffer;
    juce::AudioBuffer<float> harmonyBuffer;

    // Schmitt-trigger note latch
    int currentLockedMidi = -1;
    int unvoicedFrameCount = 0;

    // Retune speed smoothing filter
    float smoothedShiftSemitones = 0.0f;
    float trackedPitchHz = 220.0f;
    bool wasVoicedLast = false;

    // Display state (atomic)
    std::atomic<float> displayCentDeviation { 0.0f };
    std::atomic<int> displayNoteInOctave { 3 }; // D#
    std::atomic<int> displayOctave { 3 };       // 3
    std::atomic<bool> displayIsVoiced { false };
    std::atomic<float> displayCurrentPitch { 0.0f };
    std::atomic<float> displayTargetPitch { 0.0f };
    std::atomic<uint16_t> displayActiveMask { 0 };

    float smoothedCentDisplay = 0.0f;
};

} // namespace VocalTune
