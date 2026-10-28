#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PitchDetector.h"
#include "PitchShifter.h"
#include "FormantShifter.h"
#include "VibratoGenerator.h"
#include "../Common/ScaleDefinition.h"
#include <atomic>
#include <array>

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

    // The control clock is independent of the host's audio-buffer boundaries.
    static constexpr int controlInterval = 32;
    std::array<float, controlInterval> monoAnalysisBuffer {};
    std::array<std::array<float, controlInterval>, 2> harmonyBuffer {};
    int samplesUntilControlUpdate = 0;
    float currentHarmonyRatio = 1.0f;
    float harmonyTargetMix = 0.0f, smoothedHarmonyMix = 0.0f;
    float harmonyMixCoefficient = 0.0f;
    bool harmonyRunning = false;
    int harmonyWarmup = 0;
    float currentPitchRatio = 1.0f;
    float currentPeriodSamples = 48000.0f / 220.0f;
    bool currentVoiced = false;

    // Schmitt-trigger note latch for eliminating note flicker / trill
    int currentLockedMidi = -1;
    int unvoicedSamples = 0;

    // Retune speed smoothing
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

    void updateControls(float correctionPercent, float retuneSpeedMs, float flexTunePercent,
                        float vibratoPercent, float harmonizeMix, int harmonyIntervalIndex,
                        float tuningRef, uint16_t activeMask);
};

} // namespace VocalTune
