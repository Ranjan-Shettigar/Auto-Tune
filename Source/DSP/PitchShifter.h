#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>

namespace VocalTune
{

// Streaming, stereo-linked spectral pitch shifting. Configuration and all
// allocation happen in prepare(); process() accepts arbitrary host block sizes.
class PitchShifter
{
public:
    PitchShifter();
    ~PitchShifter();

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Selects the configuration for the NEXT prepare(). Changing the latency
    // of an active stream would discard audio and invalidate host compensation.
    void setLowLatencyMode(bool lowLatency) { requestedLowLatency = lowLatency; }

    void process(juce::AudioBuffer<float>& buffer, float targetPitchRatio,
                 float pitchPeriodSamples, bool isVoiced);
    int getLatencySamples() const { return latencySamples; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    static constexpr int quantum = 32;
    std::array<std::array<float, quantum>, 2> input {};
    std::array<std::array<float, quantum>, 2> output {};
    int fifoPosition = 0;
    int numChannels = 2;
    int latencySamples = 0;
    bool requestedLowLatency = true;
    bool prepared = false;
    float smoothedSemitones = 0.0f;
    float ratioCoefficient = 1.0f;
};

} // namespace VocalTune
