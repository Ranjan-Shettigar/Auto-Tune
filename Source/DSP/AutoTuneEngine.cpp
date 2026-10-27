#include "AutoTuneEngine.h"
#include <cmath>
#include <algorithm>

namespace VocalTune
{

AutoTuneEngine::AutoTuneEngine()
{
}

void AutoTuneEngine::prepare(double sampleRate, int maxBlockSize, int channels)
{
    currentSampleRate = (sampleRate > 8000.0) ? sampleRate : 48000.0;
    numChannels = std::max(1, channels);

    monoAnalysisBuffer.setSize(1, maxBlockSize);
    harmonyBuffer.setSize(numChannels, maxBlockSize);

    pitchDetector.prepare(currentSampleRate, maxBlockSize);
    pitchShifter.prepare(currentSampleRate, maxBlockSize, numChannels);
    harmonyShifter.prepare(currentSampleRate, maxBlockSize, numChannels);
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
    formantShifter.reset();
    vibratoGenerator.reset();

    currentLockedMidi = -1;
    unvoicedFrameCount = 0;
    smoothedShiftSemitones = 0.0f;
    trackedPitchHz = 220.0f;
    wasVoicedLast = false;
    smoothedCentDisplay = 0.0f;

    displayCentDeviation.store(0.0f);
    displayIsVoiced.store(false);
}

void AutoTuneEngine::process(juce::AudioBuffer<float>& buffer,
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
                             uint16_t customMask)
{
    const int numSamples = buffer.getNumSamples();
    const int totalChannels = std::min(numChannels, buffer.getNumChannels());

    if (numSamples <= 0 || totalChannels <= 0)
        return;

    // Configure pitch detector ranges
    pitchDetector.setVocalRange(vocalRangeIndex);
    pitchDetector.setTrackingSensitivity(trackingSensIndex);

    // Fast mono mix for pitch detection
    monoAnalysisBuffer.setSize(1, numSamples, false, false, true);
    auto* monoData = monoAnalysisBuffer.getWritePointer(0);

    if (totalChannels == 1)
    {
        monoAnalysisBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    }
    else
    {
        const auto* left = buffer.getReadPointer(0);
        const auto* right = buffer.getReadPointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            monoData[i] = 0.5f * (left[i] + right[i]);
        }
    }

    // Determine active note mask from root and scale type
    uint16_t activeMask = 0x0FFF;
    if (scaleType == ScaleType::Custom)
    {
        activeMask = customMask;
    }
    else if (followScale)
    {
        activeMask = computeActiveNotesMask(rootNote, scaleType);
    }

    displayActiveMask.store(activeMask);

    // Run pitch detection
    PitchDetectionResult pitchResult;
    pitchDetector.processBlock(monoData, numSamples, pitchResult);

    float targetShiftSemitones = 0.0f;
    bool isVoiced = pitchResult.isVoiced && (pitchResult.pitchHz >= 45.0f);

    if (isVoiced)
    {
        unvoicedFrameCount = 0;
        float detectedPitch = pitchResult.pitchHz;
        float pitchDiff = std::abs(detectedPitch - trackedPitchHz) / trackedPitchHz;
        float trackAlpha = (pitchDiff > 0.08f) ? 0.60f : 0.25f;
        trackedPitchHz += trackAlpha * (detectedPitch - trackedPitchHz);

        // Snap pitch with hysteresis
        ScaleSnapResult snap = snapPitchWithHysteresis(trackedPitchHz, activeMask, currentLockedMidi, tuningRef, 0.18f);

        float rawErrorSemitones = -(snap.centDeviation / 100.0f);

        // Flex-Tune modulation: when flexTune is active, wide pitch deviations are pulled less
        float flexFactor = 1.0f;
        if (flexTunePercent > 0.5f)
        {
            float normDev = std::abs(snap.centDeviation) / 50.0f;
            flexFactor = juce::jlimit(0.0f, 1.0f, 1.0f - (flexTunePercent / 100.0f) * (normDev * normDev));
        }

        float correctionDepth = juce::jlimit(0.0f, 1.0f, correctionPercent / 100.0f);
        targetShiftSemitones = rawErrorSemitones * correctionDepth * flexFactor;

        // Retune Speed time constant
        float alpha;
        if (retuneSpeedMs <= 0.5f)
        {
            alpha = 1.0f; // Instant snap
        }
        else
        {
            float tau = std::max(0.001f, retuneSpeedMs * 0.001f);
            float dt = static_cast<float>(numSamples) / static_cast<float>(currentSampleRate);
            alpha = 1.0f - std::exp(-dt / tau);
            alpha = juce::jlimit(0.02f, 1.0f, alpha);
        }

        if (!wasVoicedLast)
        {
            smoothedShiftSemitones = targetShiftSemitones;
        }
        else
        {
            smoothedShiftSemitones += alpha * (targetShiftSemitones - smoothedShiftSemitones);
        }

        wasVoicedLast = true;

        // Update UI states
        displayIsVoiced.store(true);
        displayNoteInOctave.store(snap.targetNoteInOctave);
        displayOctave.store(snap.targetOctave);
        displayCurrentPitch.store(trackedPitchHz);
        displayTargetPitch.store(snap.targetFrequency);

        smoothedCentDisplay += 0.35f * (snap.centDeviation - smoothedCentDisplay);
        displayCentDeviation.store(smoothedCentDisplay);
    }
    else
    {
        unvoicedFrameCount++;
        if (unvoicedFrameCount > 8)
        {
            currentLockedMidi = -1;
        }

        targetShiftSemitones = 0.0f;
        smoothedShiftSemitones *= 0.80f;
        wasVoicedLast = false;

        displayIsVoiced.store(false);
        smoothedCentDisplay *= 0.80f;
        displayCentDeviation.store(smoothedCentDisplay);
    }

    // Add vibrato modulation
    float vibratoRatio = vibratoGenerator.getNextRatio(vibratoPercent, numSamples);
    float vibratoSemitones = (std::abs(vibratoRatio - 1.0f) > 1e-4f) ? (12.0f * std::log2(vibratoRatio)) : 0.0f;

    float finalShiftSemitones = smoothedShiftSemitones + vibratoSemitones;
    finalShiftSemitones = juce::jlimit(-12.0f, 12.0f, finalShiftSemitones);

    float targetPitchRatio = std::pow(2.0f, finalShiftSemitones / 12.0f);
    float fundamentalPeriodSamples = static_cast<float>(currentSampleRate) / (trackedPitchHz > 45.0f ? trackedPitchHz : 220.0f);

    // Prepare harmony buffer if harmonize is active
    float harmMix = juce::jlimit(0.0f, 1.0f, harmonizeMix / 100.0f);
    if (harmMix > 0.01f)
    {
        harmonyBuffer.setSize(totalChannels, numSamples, false, false, true);
        for (int ch = 0; ch < totalChannels; ++ch)
        {
            harmonyBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }

        // Harmony intervals: 0: +4 st (+3rd), 1: +7 st (+5th), 2: +12 st (+Oct), 3: -4 st (-3rd), 4: -5 st (-4th), 5: -12 st (-Oct)
        static const float harmonyOffsets[6] = { 4.0f, 7.0f, 12.0f, -4.0f, -5.0f, -12.0f };
        float harmOffset = harmonyOffsets[juce::jlimit(0, 5, harmonyIntervalIndex)];
        float harmShiftSemitones = juce::jlimit(-12.0f, 12.0f, finalShiftSemitones + harmOffset);
        float harmRatio = std::pow(2.0f, harmShiftSemitones / 12.0f);

        harmonyShifter.process(harmonyBuffer, harmRatio, fundamentalPeriodSamples, isVoiced);
    }

    // Process primary pitch shifter
    pitchShifter.process(buffer, targetPitchRatio, fundamentalPeriodSamples, isVoiced);

    // Apply formant shift
    formantShifter.process(buffer, formantShiftSemitones);

    // Blend harmony if active
    if (harmMix > 0.01f)
    {
        float dryGain = 1.0f - 0.4f * harmMix;
        float harmGain = harmMix * 0.85f;

        for (int ch = 0; ch < totalChannels; ++ch)
        {
            auto* outData = buffer.getWritePointer(ch);
            const auto* hData = harmonyBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                outData[i] = dryGain * outData[i] + harmGain * hData[i];
            }
        }
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
