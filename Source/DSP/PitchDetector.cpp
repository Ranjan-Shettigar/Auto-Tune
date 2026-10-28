#include "PitchDetector.h"
#include <cmath>
#include <algorithm>

namespace VocalTune
{

PitchDetector::PitchDetector() = default;

void PitchDetector::setLowLatencyMode(bool lowLatency)
{
    isLowLatency = lowLatency;
    // Changing cadence must not erase a phrase that is already being tracked.
    hopSize = std::max(1, static_cast<int>(std::lround(analysisSampleRate
                                                     * (lowLatency ? 128.0 : 256.0) / 48000.0)));
    samplesSinceLastAnalysis = std::min(samplesSinceLastAnalysis, hopSize - 1);
}

void PitchDetector::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = (std::isfinite(sampleRate) && sampleRate >= 8000.0
                        && sampleRate <= 768000.0) ? sampleRate : 48000.0;

    analysisSampleRate = currentSampleRate;
    decimationStages = 0;
    while (analysisSampleRate > 12000.0 && decimationStages < static_cast<int>(decimators.size()))
    {
        analysisSampleRate *= 0.5;
        ++decimationStages;
    }
    // Symmetric 31-tap half-band FIR; zero odd taps leave eight pairs and a centre tap.
    double sum = 0.5;
    for (int pair = 0; pair < 8; ++pair)
    {
        const int tap = 2 * pair;
        const double distance = tap - 15.0;
        const double angle = juce::MathConstants<double>::twoPi * tap / 30.0;
        const double window = 0.42 - 0.5 * std::cos(angle) + 0.08 * std::cos(2 * angle);
        const double coefficient = window * std::sin(juce::MathConstants<double>::halfPi * distance)
                                   / (juce::MathConstants<double>::pi * distance);
        halfBandPairs[static_cast<size_t>(pair)] = static_cast<float>(coefficient);
        sum += 2 * coefficient;
    }
    halfBandCentre = static_cast<float>(0.5 / sum);
    for (auto& coefficient : halfBandPairs) coefficient /= static_cast<float>(sum);
    const int requiredWindow = static_cast<int>(std::ceil(2.0 * analysisSampleRate / minimumPitchHz)) + 8;
    // Keep just over two 45 Hz periods instead of rounding the analysis
    // duration up to 85 ms. Only the zero-padded FFT must be a power of two.
    windowSize = requiredWindow;
    int paddedWindow = 1;
    int windowOrder = 0;
    while (paddedWindow < windowSize)
    {
        paddedWindow *= 2;
        ++windowOrder;
    }
    fftSize = 2 * paddedWindow;
    fft = std::make_unique<juce::dsp::FFT>(windowOrder + 1);
    circularBuffer.assign(static_cast<size_t>(windowSize), 0.0f);
    analysisBuffer.assign(static_cast<size_t>(windowSize), 0.0f);
    fftBuffer.assign(static_cast<size_t>(fftSize * 2), 0.0f);
    cumulativeEnergy.assign(static_cast<size_t>(windowSize + 1), 0.0f);
    nsdf.assign(static_cast<size_t>(windowSize / 2), 0.0f);
    setLowLatencyMode(isLowLatency);
    reset();
}

void PitchDetector::clearPitchHistory()
{
    pitchHistory.fill(0.0f);
    historyIndex = 0;
    historyCount = 0;
}

void PitchDetector::reset()
{
    for (auto& stage : decimators) stage = {};
    writeIndex = 0;
    samplesSinceLastAnalysis = 0;
    samplesInBuffer = 0;
    unvoicedSamples = 0;
    std::fill(circularBuffer.begin(), circularBuffer.end(), 0.0f);
    clearPitchHistory();
    latestResult = {};
}

void PitchDetector::setVocalRange(int index)
{
    index = juce::jlimit(0, 3, index);
    if (vocalRange == index) return;
    vocalRange = index;
    static constexpr float minimum[] { 120, 180, 50, 45 };
    static constexpr float maximum[] { 800, 1200, 450, 1500 };
    minPitch = minimum[index];
    maxPitch = maximum[index];
    clearPitchHistory();
    latestResult = {};
}

bool PitchDetector::decimate(float& sample)
{
    for (int i = 0; i < decimationStages; ++i)
    {
        auto& stage = decimators[static_cast<size_t>(i)];
        const int newest = stage.writeIndex;
        stage.history[static_cast<size_t>(newest)] = sample;
        stage.writeIndex = (newest + 1) & 31;
        stage.outputNext = !stage.outputNext;
        if (stage.outputNext) return false;
        float filtered = halfBandCentre * stage.history[static_cast<size_t>((newest - 15) & 31)];
        for (int pair = 0; pair < 8; ++pair)
        {
            const int tap = 2 * pair;
            filtered += halfBandPairs[static_cast<size_t>(pair)]
                * (stage.history[static_cast<size_t>((newest - tap) & 31)]
                   + stage.history[static_cast<size_t>((newest - 30 + tap) & 31)]);
        }
        sample = filtered;
    }
    return true;
}

bool PitchDetector::processBlock(const float* channelData, int numSamples, PitchDetectionResult& outResult)
{
    outResult = latestResult;
    if (channelData == nullptr || numSamples <= 0 || circularBuffer.empty())
        return false;

    bool updated = false;
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = std::isfinite(channelData[i]) ? channelData[i] : 0.0f;
        if (!decimate(sample)) continue;
        circularBuffer[static_cast<size_t>(writeIndex)] = sample;
        if (++writeIndex == windowSize)
            writeIndex = 0;
        samplesInBuffer = std::min(samplesInBuffer + 1, windowSize);

        if (++samplesSinceLastAnalysis >= hopSize)
        {
            samplesSinceLastAnalysis = 0;
            // Zero-filled startup windows can produce convincing, incorrect periods.
            if (samplesInBuffer < windowSize)
                continue;

            const auto firstPart = windowSize - writeIndex;
            std::copy_n(circularBuffer.begin() + writeIndex, firstPart, analysisBuffer.begin());
            std::copy_n(circularBuffer.begin(), writeIndex, analysisBuffer.begin() + firstPart);

            latestResult = analyzeWindow();
            if (latestResult.isVoiced)
            {
                unvoicedSamples = 0;
            }
            else
            {
                const int historyTimeout = static_cast<int>(analysisSampleRate * 0.025);
                unvoicedSamples = std::min(unvoicedSamples + hopSize, historyTimeout);
                if (unvoicedSamples >= historyTimeout)
                    clearPitchHistory();
            }
            outResult = latestResult;
            updated = true;
        }
    }
    return updated;
}

void PitchDetector::computeNSDFFFT()
{
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    std::copy(analysisBuffer.begin(), analysisBuffer.end(), fftBuffer.begin());

    cumulativeEnergy[0] = 0.0f;
    for (int i = 0; i < windowSize; ++i)
        cumulativeEnergy[static_cast<size_t>(i + 1)] = cumulativeEnergy[static_cast<size_t>(i)]
            + analysisBuffer[static_cast<size_t>(i)] * analysisBuffer[static_cast<size_t>(i)];
    const float totalEnergy = cumulativeEnergy[static_cast<size_t>(windowSize)];

    fft->performRealOnlyForwardTransform(fftBuffer.data());
    const int numBins = fftSize / 2 + 1;
    for (int k = 0; k < numBins; ++k)
    {
        const auto index = static_cast<size_t>(2 * k);
        const float re = fftBuffer[index];
        const float im = fftBuffer[index + 1];
        fftBuffer[index] = re * re + im * im;
        fftBuffer[index + 1] = 0.0f;
    }
    fft->performRealOnlyInverseTransform(fftBuffer.data());

    const float r0 = fftBuffer[0];
    const float invScale = (totalEnergy > 1.0e-12f && r0 > 1.0e-12f) ? totalEnergy / r0 : 1.0f;
    for (int tau = 0; tau < windowSize / 2; ++tau)
    {
        const float denom = cumulativeEnergy[static_cast<size_t>(windowSize - tau)]
                            + totalEnergy - cumulativeEnergy[static_cast<size_t>(tau)];
        nsdf[static_cast<size_t>(tau)] = denom > 1.0e-12f
            ? juce::jlimit(-1.0f, 1.0f, 2.0f * fftBuffer[static_cast<size_t>(tau)] * invScale / denom)
            : 0.0f;
    }
}

PitchDetectionResult PitchDetector::analyzeWindow()
{
    PitchDetectionResult result;

    // A DC offset is not periodic vocal energy; remove it before RMS and correlation.
    double sum = 0.0;
    for (float sample : analysisBuffer)
        sum += sample;
    const float mean = static_cast<float>(sum / windowSize);
    double sumSq = 0.0;
    for (auto& sample : analysisBuffer)
    {
        sample -= mean;
        sumSq += static_cast<double>(sample) * sample;
    }
    result.rmsLevel = static_cast<float>(std::sqrt(sumSq / windowSize));
    if (result.rmsLevel < 0.003f)
        return result;

    computeNSDFFFT();

    const int minPeriod = std::max(2, static_cast<int>(analysisSampleRate / maxPitch));
    const int maxPeriod = std::min(windowSize / 2 - 2,
                                  static_cast<int>(std::ceil(analysisSampleRate / minPitch)));

    // Ignore the positive lobe attached to zero lag. It represents the waveform's
    // width, not its repetition period, and is particularly misleading for breaths.
    int firstNegative = 1;
    while (firstNegative < maxPeriod && nsdf[static_cast<size_t>(firstNegative)] > 0.0f)
        ++firstNegative;
    const int searchStart = std::max(minPeriod, firstNegative);

    auto isPeak = [this](int tau)
    {
        const auto index = static_cast<size_t>(tau);
        return nsdf[index] > 0.0f && nsdf[index] > nsdf[index - 1] && nsdf[index] >= nsdf[index + 1];
    };
    auto interpolatePeak = [this](int tau, float& peakTau)
    {
        const auto index = static_cast<size_t>(tau);
        const float y0 = nsdf[index - 1];
        const float y1 = nsdf[index];
        const float y2 = nsdf[index + 1];
        const float denom = 2.0f * (2.0f * y1 - y0 - y2);
        const float delta = std::abs(denom) > 1.0e-6f
            ? juce::jlimit(-0.5f, 0.5f, (y2 - y0) / denom) : 0.0f;
        peakTau = static_cast<float>(tau) + delta;
        return juce::jlimit(0.0f, 1.0f, y1 + 0.25f * (y2 - y0) * delta);
    };

    // Two bounded scans replace the allocating vector previously used at every hop.
    float highestPeakVal = 0.0f;
    for (int tau = searchStart; tau <= maxPeriod; ++tau)
    {
        if (isPeak(tau))
        {
            float ignoredTau = 0.0f;
            highestPeakVal = std::max(highestPeakVal, interpolatePeak(tau, ignoredTau));
        }
    }

    const float sensitivity = trackingSensitivity == 1 ? -0.10f : trackingSensitivity == 2 ? 0.08f : 0.0f;
    const float voicingThreshold = (latestResult.isVoiced ? 0.60f : 0.72f) + sensitivity;
    result.confidence = highestPeakVal;
    if (highestPeakVal < voicingThreshold)
        return result;

    const float threshold = std::max(voicingThreshold, 0.90f * highestPeakVal);
    float selectedTau = 0.0f;
    float selectedVal = 0.0f;
    for (int tau = searchStart; tau <= maxPeriod; ++tau)
    {
        if (isPeak(tau))
        {
            float peakTau = 0.0f;
            const float peakVal = interpolatePeak(tau, peakTau);
            if (peakVal >= threshold)
            {
                selectedTau = peakTau;
                selectedVal = peakVal;
                break;
            }
        }
    }
    if (selectedTau <= 0.0f)
        return result;

    const float detectedPitch = static_cast<float>(analysisSampleRate) / selectedTau;
    if (detectedPitch < minPitch * 0.99f || detectedPitch > maxPitch * 1.01f)
        return result;

    pitchHistory[static_cast<size_t>(historyIndex)] = detectedPitch;
    historyIndex = (historyIndex + 1) % static_cast<int>(pitchHistory.size());
    historyCount = std::min(historyCount + 1, static_cast<int>(pitchHistory.size()));
    auto sorted = pitchHistory;
    std::sort(sorted.begin(), sorted.begin() + historyCount);

    result.pitchHz = sorted[static_cast<size_t>(historyCount / 2)];
    result.confidence = selectedVal;
    result.isVoiced = true;
    return result;
}

} // namespace VocalTune
