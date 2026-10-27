#include "PitchDetector.h"
#include <cmath>
#include <algorithm>

namespace VocalTune
{

PitchDetector::PitchDetector()
    : fft(fftOrder)
{
    setLowLatencyMode(true);
}

void PitchDetector::setLowLatencyMode(bool lowLatency)
{
    isLowLatency = lowLatency;
    hopSize = lowLatency ? 128 : 256;
    reset();
}

void PitchDetector::setVocalRange(int rangeIndex)
{
    vocalRangeIndex = juce::jlimit(0, 3, rangeIndex);
}

void PitchDetector::setTrackingSensitivity(int sensIndex)
{
    trackingSensIndex = juce::jlimit(0, 2, sensIndex);
}

void PitchDetector::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = (sampleRate > 8000.0) ? sampleRate : 48000.0;
    bufferSize = 8192;
    circularBuffer.assign(bufferSize, 0.0f);
    analysisBuffer.assign(windowSize, 0.0f);
    fftBuffer.assign(fftSize * 2, 0.0f);
    cumulativeEnergy.assign(windowSize + 1, 0.0f);
    nsdf.assign(windowSize, 0.0f);
    reset();
}

void PitchDetector::reset()
{
    writeIndex = 0;
    samplesSinceLastAnalysis = 0;
    std::fill(circularBuffer.begin(), circularBuffer.end(), 0.0f);
    std::fill(pitchHistory, pitchHistory + 5, 0.0f);
    historyIndex = 0;
    latestResult = PitchDetectionResult();
}

bool PitchDetector::processBlock(const float* channelData, int numSamples, PitchDetectionResult& outResult)
{
    if (channelData == nullptr || numSamples <= 0)
        return false;

    bool updated = false;

    for (int i = 0; i < numSamples; ++i)
    {
        circularBuffer[writeIndex] = channelData[i];
        writeIndex = (writeIndex + 1) % bufferSize;

        samplesSinceLastAnalysis++;
        if (samplesSinceLastAnalysis >= hopSize)
        {
            samplesSinceLastAnalysis = 0;

            int readIdx = (writeIndex - windowSize + bufferSize) % bufferSize;
            for (int j = 0; j < windowSize; ++j)
            {
                analysisBuffer[j] = circularBuffer[readIdx];
                readIdx = (readIdx + 1) % bufferSize;
            }

            latestResult = analyzeWindow();
            outResult = latestResult;
            updated = true;
        }
    }

    if (!updated)
        outResult = latestResult;

    return updated;
}

void PitchDetector::computeNSDFFFT()
{
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    std::copy(analysisBuffer.begin(), analysisBuffer.end(), fftBuffer.begin());

    cumulativeEnergy[0] = 0.0f;
    for (int i = 0; i < windowSize; ++i)
    {
        cumulativeEnergy[i + 1] = cumulativeEnergy[i] + analysisBuffer[i] * analysisBuffer[i];
    }
    const float totalEnergy = cumulativeEnergy[windowSize];

    fft.performRealOnlyForwardTransform(fftBuffer.data());

    const int numBins = (fftSize / 2) + 1;
    for (int k = 0; k < numBins; ++k)
    {
        float re = fftBuffer[2 * k];
        float im = fftBuffer[2 * k + 1];
        fftBuffer[2 * k]     = re * re + im * im;
        fftBuffer[2 * k + 1] = 0.0f;
    }

    fft.performRealOnlyInverseTransform(fftBuffer.data());

    float r0 = fftBuffer[0];
    float scale = (totalEnergy > 1e-9f && r0 > 1e-9f) ? (r0 / totalEnergy) : 1.0f;
    float invScale = 1.0f / scale;

    const int maxLag = windowSize / 2;
    for (int tau = 0; tau < maxLag; ++tau)
    {
        float r_tau = fftBuffer[tau] * invScale;
        float denom = cumulativeEnergy[windowSize - tau] + (totalEnergy - cumulativeEnergy[tau]);

        if (denom > 1e-9f)
            nsdf[tau] = (2.0f * r_tau) / denom;
        else
            nsdf[tau] = 0.0f;
    }
}

PitchDetectionResult PitchDetector::analyzeWindow()
{
    PitchDetectionResult result;

    float sumSq = 0.0f;
    for (int i = 0; i < windowSize; ++i)
        sumSq += analysisBuffer[i] * analysisBuffer[i];

    result.rmsLevel = std::sqrt(sumSq / static_cast<float>(windowSize));

    if (result.rmsLevel < 0.003f)
    {
        result.isVoiced = false;
        result.pitchHz = 0.0f;
        result.confidence = 0.0f;
        return result;
    }

    computeNSDFFFT();

    // Set frequency bounds based on selected vocal / instrument range
    float minHz = 120.0f;
    float maxHz = 800.0f;

    switch (vocalRangeIndex)
    {
        case 1: // Soprano
            minHz = 180.0f; maxHz = 1200.0f; break;
        case 2: // Low Male / Bass
            minHz = 50.0f;  maxHz = 450.0f;  break;
        case 3: // Instrument
            minHz = 45.0f;  maxHz = 1500.0f; break;
        case 0: // Alto / Tenor
        default:
            minHz = 120.0f; maxHz = 800.0f;  break;
    }

    int maxLag = windowSize / 2;
    int minPeriod = static_cast<int>(currentSampleRate / maxHz);
    int maxPeriod = static_cast<int>(currentSampleRate / minHz);

    if (maxPeriod >= maxLag)
        maxPeriod = maxLag - 2;
    if (minPeriod < 2)
        minPeriod = 2;

    struct Peak
    {
        float tau;
        float value;
    };
    std::vector<Peak> peaks;
    peaks.reserve(32);

    float highestPeakVal = 0.0f;
    bool positive = false;

    for (int tau = minPeriod; tau < maxPeriod; ++tau)
    {
        if (nsdf[tau] > 0.0f && !positive)
            positive = true;
        else if (nsdf[tau] <= 0.0f && positive)
            positive = false;

        if (positive)
        {
            if (nsdf[tau] > nsdf[tau - 1] && nsdf[tau] >= nsdf[tau + 1])
            {
                float y0 = nsdf[tau - 1];
                float y1 = nsdf[tau];
                float y2 = nsdf[tau + 1];

                float denom = 2.0f * (2.0f * y1 - y0 - y2);
                float delta = 0.0f;
                if (std::abs(denom) > 1e-6f)
                    delta = (y2 - y0) / denom;

                float peakTau = static_cast<float>(tau) + delta;
                float peakVal = y1 + 0.25f * (y2 - y0) * delta;

                if (peakVal > highestPeakVal)
                    highestPeakVal = peakVal;

                peaks.push_back({ peakTau, peakVal });
            }
        }
    }

    // Set voicing threshold based on tracking sensitivity
    float voiceThresh = 0.50f;
    if (trackingSensIndex == 1)      voiceThresh = 0.40f; // Relaxed
    else if (trackingSensIndex == 2) voiceThresh = 0.62f; // Strict

    if (peaks.empty() || highestPeakVal < voiceThresh)
    {
        result.isVoiced = false;
        result.pitchHz = 0.0f;
        result.confidence = highestPeakVal;
        return result;
    }

    float threshold = 0.85f * highestPeakVal;
    float selectedTau = 0.0f;
    float selectedVal = 0.0f;

    for (const auto& p : peaks)
    {
        if (p.value >= threshold)
        {
            selectedTau = p.tau;
            selectedVal = p.value;
            break;
        }
    }

    if (selectedTau <= 0.0f)
    {
        result.isVoiced = false;
        return result;
    }

    float detectedPitch = static_cast<float>(currentSampleRate) / selectedTau;

    if (detectedPitch < minHz * 0.9f || detectedPitch > maxHz * 1.1f)
    {
        result.isVoiced = false;
        return result;
    }

    pitchHistory[historyIndex] = detectedPitch;
    historyIndex = (historyIndex + 1) % 5;

    float sorted[5];
    std::copy(pitchHistory, pitchHistory + 5, sorted);
    std::sort(sorted, sorted + 5);

    int nonZeroCount = 0;
    float nonZeroSorted[5];
    for (int i = 0; i < 5; ++i)
    {
        if (sorted[i] > 40.0f)
            nonZeroSorted[nonZeroCount++] = sorted[i];
    }

    float stablePitch = detectedPitch;
    if (nonZeroCount >= 3)
    {
        stablePitch = nonZeroSorted[nonZeroCount / 2];
    }

    result.pitchHz = stablePitch;
    result.confidence = selectedVal;
    result.isVoiced = (selectedVal >= voiceThresh);

    return result;
}

} // namespace VocalTune
