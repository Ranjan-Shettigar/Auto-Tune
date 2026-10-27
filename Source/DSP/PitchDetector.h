#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

namespace VocalTune
{

struct PitchDetectionResult
{
    float pitchHz = 0.0f;
    float confidence = 0.0f;
    bool isVoiced = false;
    float rmsLevel = 0.0f;
};

class PitchDetector
{
public:
    PitchDetector();
    ~PitchDetector() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setLowLatencyMode(bool lowLatency);

    bool processBlock(const float* channelData, int numSamples, PitchDetectionResult& outResult);

    PitchDetectionResult getLatestResult() const { return latestResult; }

private:
    double currentSampleRate = 48000.0;
    bool isLowLatency = true;

    // Keep at least two periods of the lowest supported note at every sample rate.
    // FFT creation and all buffer allocation happen in prepare(), never in processing.
    static constexpr float minimumPitchHz = 55.0f;
    static constexpr float maximumPitchHz = 1100.0f;
    int windowSize = 2048;
    int fftSize = 4096;

    int hopSize = 256;
    int samplesSinceLastAnalysis = 0;
    int samplesInBuffer = 0;

    std::vector<float> circularBuffer;
    int writeIndex = 0;

    std::vector<float> analysisBuffer;
    std::vector<float> fftBuffer;
    std::vector<float> cumulativeEnergy;
    std::vector<float> nsdf;

    std::unique_ptr<juce::dsp::FFT> fft;

    PitchDetectionResult latestResult;

    // 5-point median filter for outlier rejection
    std::array<float, 5> pitchHistory {};
    int historyIndex = 0;
    int historyCount = 0;
    int unvoicedSamples = 0;

    PitchDetectionResult analyzeWindow();
    void computeNSDFFFT();
    void clearPitchHistory();
};

} // namespace VocalTune
