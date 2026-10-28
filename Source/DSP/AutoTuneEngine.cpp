#include "AutoTuneEngine.h"
#include <cmath>
#include <algorithm>

namespace VocalTune
{

AutoTuneEngine::AutoTuneEngine() = default;

void AutoTuneEngine::prepare(double sampleRate, int maxBlockSize, int channels)
{
    currentSampleRate = (sampleRate > 8000.0) ? sampleRate : 48000.0;
    // The plugin supports mono and stereo buses.
    numChannels = juce::jlimit(1, 2, channels);
    pitchDetector.prepare(currentSampleRate, maxBlockSize);
    pitchShifter.prepare(currentSampleRate, maxBlockSize, numChannels);
    harmonyShifter.prepare(currentSampleRate, maxBlockSize, numChannels);
    harmonyMixCoefficient = -std::expm1(-1.0f / static_cast<float>(0.020 * currentSampleRate));
    formantShifter.prepare(currentSampleRate, maxBlockSize, numChannels);
    vibratoGenerator.prepare(currentSampleRate);
    reset();
}

void AutoTuneEngine::setLowLatencyMode(bool lowLatency)
{
    isLowLatency = lowLatency;
    pitchDetector.setLowLatencyMode(lowLatency);
    pitchShifter.setLowLatencyMode(lowLatency);
    harmonyShifter.setLowLatencyMode(lowLatency);
}

void AutoTuneEngine::reset()
{
    pitchDetector.reset();
    pitchShifter.reset();
    harmonyShifter.reset();
    harmonyRunning = false;
    harmonyWarmup = 0;
    smoothedHarmonyMix = harmonyTargetMix = 0.0f;
    formantShifter.reset();
    vibratoGenerator.reset();
    currentLockedMidi = -1;
    unvoicedSamples = 0;
    samplesUntilControlUpdate = 0;
    currentPitchRatio = 1.0f;
    currentPeriodSamples = static_cast<float>(currentSampleRate) / 220.0f;
    currentVoiced = false;
    smoothedShiftSemitones = 0.0f;
    trackedPitchHz = 220.0f;
    wasVoicedLast = false;
    smoothedCentDisplay = 0.0f;
    displayCentDeviation.store(0.0f);
    displayIsVoiced.store(false);
    displayCurrentPitch.store(0.0f);
    displayTargetPitch.store(0.0f);
}

void AutoTuneEngine::updateControls(float correctionPercent, float retuneSpeedMs, float flexTunePercent,
                                  float vibratoPercent, float harmonizeMix, int harmonyIntervalIndex,
                                  float tuningRef, uint16_t activeMask)
{
    const auto pitchResult = pitchDetector.getLatestResult();
    currentVoiced = pitchResult.isVoiced && std::isfinite(pitchResult.pitchHz)
                    && pitchResult.pitchHz >= 44.0f;
    const float dt = static_cast<float>(controlInterval / currentSampleRate);
    const auto coefficient = [dt](float seconds) { return -std::expm1(-dt / seconds); };
    const float correctionDepth = juce::jlimit(0.0f, 1.0f, correctionPercent * 0.01f);
    float targetShiftSemitones = 0.0f;

    if (currentVoiced)
    {
        unvoicedSamples = 0;
        // Start each phrase at its measured pitch, rather than gliding from
        // 220 Hz or the preceding phrase. Smooth in musical (log-frequency) space.
        if (!wasVoicedLast)
            trackedPitchHz = pitchResult.pitchHz;
        else
        {
            const float pitchError = std::log2(pitchResult.pitchHz / trackedPitchHz);
            const float trackingTime = std::abs(pitchError) > 0.12f ? 0.002f : 0.008f;
            trackedPitchHz *= std::exp2(coefficient(trackingTime) * pitchError);
        }

        const auto snap = snapPitchWithHysteresis(trackedPitchHz, activeMask,
                                                  currentLockedMidi, tuningRef, 0.18f);
        if (snap.isVoiced)
        {
            // centDeviation is clipped for the meter; it is not the full error
            // when a sparse scale requires more than a semitone of correction.
            const float error = 12.0f * std::log2(snap.targetFrequency / trackedPitchHz);
            const float deviation = std::abs(error) * 2.0f;
            const float flexFactor = juce::jlimit(0.0f, 1.0f,
                1.0f - juce::jlimit(0.0f, 1.0f, flexTunePercent * 0.01f) * deviation * deviation);
            targetShiftSemitones = error * correctionDepth * flexFactor;
            displayNoteInOctave.store(snap.targetNoteInOctave);
            displayOctave.store(snap.targetOctave);
            displayTargetPitch.store(snap.targetFrequency);
            smoothedCentDisplay += coefficient(0.025f) * (snap.centDeviation - smoothedCentDisplay);
        }
        else
        {
            displayTargetPitch.store(0.0f);
            smoothedCentDisplay *= 1.0f - coefficient(0.025f);
        }
        displayCurrentPitch.store(trackedPitchHz);
    }
    else
    {
        unvoicedSamples = std::min(unvoicedSamples + controlInterval,
                                   static_cast<int>(currentSampleRate));
        if (unvoicedSamples >= static_cast<int>(0.10 * currentSampleRate))
            currentLockedMidi = -1;
        smoothedCentDisplay *= 1.0f - coefficient(0.025f);
        displayCurrentPitch.store(0.0f);
        displayTargetPitch.store(0.0f);
    }

    // Time constants are in seconds, so a 32-sample or 2048-sample host buffer
    // produces the same correction. Even hard retune has a short click-free ramp.
    const float retuneSeconds = currentVoiced
        ? juce::jlimit(0.002f, 0.100f, retuneSpeedMs * 0.001f)
        : 0.020f;
    smoothedShiftSemitones += coefficient(retuneSeconds)
                             * (targetShiftSemitones - smoothedShiftSemitones);
    const float vibratoRatio = vibratoGenerator.getNextRatio(vibratoPercent, controlInterval);
    const float vibratoSemitones = 12.0f * std::log2(vibratoRatio);
    const float finalShift = juce::jlimit(-12.0f, 12.0f,
                                        smoothedShiftSemitones + vibratoSemitones);
    currentPitchRatio = std::exp2(finalShift / 12.0f);
    static constexpr float harmonyOffsets[] { 4, 7, 12, -4, -5, -12 };
    const float harmonyShift = juce::jlimit(-12.0f, 12.0f,
        finalShift + harmonyOffsets[juce::jlimit(0, 5, harmonyIntervalIndex)]);
    currentHarmonyRatio = std::exp2(harmonyShift / 12.0f);
    harmonyTargetMix = juce::jlimit(0.0f, 1.0f, harmonizeMix * 0.01f);
    if (harmonyTargetMix > 0.0f && !harmonyRunning)
    {
        harmonyShifter.reset();
        harmonyRunning = true;
        harmonyWarmup = harmonyShifter.getLatencySamples();
    }
    currentPeriodSamples = static_cast<float>(currentSampleRate) / trackedPitchHz;
    wasVoicedLast = currentVoiced;
    displayIsVoiced.store(currentVoiced);
    displayCentDeviation.store(smoothedCentDisplay);
}

void AutoTuneEngine::process(juce::AudioBuffer<float>& buffer,
                             float correctionPercent, float retuneSpeedMs, float flexTunePercent,
                             float vibratoPercent, float formantShiftSemitones,
                             float harmonizeMix, int harmonyIntervalIndex, int rootNote,
                             ScaleType scaleType, float tuningRef, int vocalRangeIndex,
                             int trackingSensIndex, bool followScale, uint16_t customMask)
{
    const int numSamples = buffer.getNumSamples();
    const int totalChannels = std::min(numChannels, buffer.getNumChannels());
    if (numSamples <= 0 || totalChannels <= 0)
        return;

    pitchDetector.setVocalRange(vocalRangeIndex);
    pitchDetector.setTrackingSensitivity(trackingSensIndex);
    retuneSpeedMs = std::isfinite(retuneSpeedMs) ? retuneSpeedMs : 15.0f;
    flexTunePercent = std::isfinite(flexTunePercent) ? flexTunePercent : 0.0f;
    harmonizeMix = std::isfinite(harmonizeMix) ? harmonizeMix : 0.0f;
    correctionPercent = std::isfinite(correctionPercent) ? correctionPercent : 0.0f;
    vibratoPercent = std::isfinite(vibratoPercent) ? vibratoPercent : 0.0f;
    tuningRef = std::isfinite(tuningRef) ? juce::jlimit(400.0f, 480.0f, tuningRef) : 440.0f;
    rootNote = (rootNote % 12 + 12) % 12;
    uint16_t activeMask = 0x0FFF;
    if (scaleType == ScaleType::Custom)
        activeMask = customMask & 0x0FFF;
    else if (followScale)
        activeMask = computeActiveNotesMask(rootNote, scaleType);
    displayActiveMask.store(activeMask);

    // Fixed-size stack storage also handles hosts exceeding their advertised
    // maximum block size without allocating in the audio callback.
    for (int offset = 0; offset < numSamples;)
    {
        if (samplesUntilControlUpdate == 0)
        {
            updateControls(correctionPercent, retuneSpeedMs, flexTunePercent, vibratoPercent,
                           harmonizeMix, harmonyIntervalIndex, tuningRef, activeMask);
            samplesUntilControlUpdate = controlInterval;
        }
        const int count = std::min(samplesUntilControlUpdate, numSamples - offset);
        float* channels[2] { buffer.getWritePointer(0, offset), nullptr };
        if (totalChannels > 1)
            channels[1] = buffer.getWritePointer(1, offset);
        for (int i = 0; i < count; ++i)
            monoAnalysisBuffer[static_cast<size_t>(i)] = totalChannels == 1
                ? channels[0][i] : 0.5f * (channels[0][i] + channels[1][i]);

        PitchDetectionResult detection;
        pitchDetector.processBlock(monoAnalysisBuffer.data(), count, detection);
        juce::AudioBuffer<float> section(channels, totalChannels, count);
        float* harmonyChannels[] { harmonyBuffer[0].data(), harmonyBuffer[1].data() };
        juce::AudioBuffer<float> harmony(harmonyChannels, totalChannels, count);
        if (harmonyRunning)
        {
            for (int ch = 0; ch < totalChannels; ++ch)
                std::copy_n(channels[ch], count, harmonyChannels[ch]);
            harmonyShifter.process(harmony, currentHarmonyRatio, currentPeriodSamples, currentVoiced);
        }
        pitchShifter.process(section, currentPitchRatio, currentPeriodSamples, currentVoiced);
        formantShifter.process(section, formantShiftSemitones);
        if (harmonyRunning)
        {
            for (int n = 0; n < count; ++n)
            {
                const float target = harmonyWarmup > 0 ? 0.0f : harmonyTargetMix;
                harmonyWarmup = std::max(0, harmonyWarmup - 1);
                smoothedHarmonyMix += harmonyMixCoefficient * (target - smoothedHarmonyMix);
                if (target == 0.0f && smoothedHarmonyMix < 1.0e-6f) smoothedHarmonyMix = 0.0f;
                for (int ch = 0; ch < totalChannels; ++ch)
                    channels[ch][n] = (1.0f - 0.4f * smoothedHarmonyMix) * channels[ch][n]
                        + 0.85f * smoothedHarmonyMix * harmonyChannels[ch][n];
            }
            if (harmonyTargetMix == 0.0f && smoothedHarmonyMix == 0.0f) harmonyRunning = false;
        }
        offset += count;
        samplesUntilControlUpdate -= count;
    }
}

VisualizerState AutoTuneEngine::getVisualizerState() const
{
    VisualizerState state;
    state.centDeviation = displayCentDeviation.load();
    state.noteInOctave = displayNoteInOctave.load();
    state.octave = displayOctave.load();
    state.isVoiced = displayIsVoiced.load();
    state.currentPitchHz = displayCurrentPitch.load();
    state.targetPitchHz = displayTargetPitch.load();
    state.activeScaleMask = displayActiveMask.load();
    return state;
}

} // namespace VocalTune
