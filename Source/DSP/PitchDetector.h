#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
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
    void setVocalRange(int rangeIndex);
    void setTrackingSensitivity(int sensIndex);

    bool processBlock(const float* channelData, int numSamples, PitchDetectionResult& outResult);

    PitchDetectionResult getLatestResult() const { return latestResult; }

private:
    double currentSampleRate = 48000.0;
    bool isLowLatency = true;

    int vocalRangeIndex = 0; // 0 = Alto/Tenor, 1 = Soprano, 2 = Low Male/Bass, 3 = Instrument
    int trackingSensIndex = 0; // 0 = Standard, 1 = Relaxed, 2 = Strict

    static constexpr int fftOrder = 12; // 4096 points
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int windowSize = 2048;

    int hopSize = 256;
    int samplesSinceLastAnalysis = 0;

    std::vector<float> circularBuffer;
    int writeIndex = 0;
    int bufferSize = 8192;

    std::vector<float> analysisBuffer;
    std::vector<float> fftBuffer;
    std::vector<float> cumulativeEnergy;
    std::vector<float> nsdf;

    juce::dsp::FFT fft;

    PitchDetectionResult latestResult;

    float pitchHistory[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int historyIndex = 0;

    PitchDetectionResult analyzeWindow();
    void computeNSDFFFT();
};

} // namespace VocalTune
