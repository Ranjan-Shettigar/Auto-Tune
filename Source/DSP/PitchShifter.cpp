#include "PitchShifter.h"
#include "../../ThirdParty/signalsmith-stretch.h"
#include <algorithm>
#include <cmath>

namespace VocalTune
{

struct PitchShifter::Impl
{
    // Fixed seed makes offline rendering reproducible.
    signalsmith::stretch::SignalsmithStretch<float> stretch { 0 };
};

PitchShifter::PitchShifter() : impl(std::make_unique<Impl>()) {}
PitchShifter::~PitchShifter() = default;

void PitchShifter::prepare(double sampleRate, int /*maxBlockSize*/, int channels)
{
    sampleRate = std::isfinite(sampleRate) && sampleRate > 8000.0 ? sampleRate : 48000.0;
    numChannels = juce::jlimit(1, 2, channels);
    // Keep the analysis context constant in seconds at every sample rate.
    // Eight overlaps provide smooth modulation and enough history for bass
    // vocals; the previous 256/512-sample windows could not do this reliably.
    const double windowSeconds = requestedLowLatency ? 0.040 : 0.080;
    const int interval = std::max(32, static_cast<int>(std::round(sampleRate * windowSeconds / 8.0)));
    impl->stretch.configure(numChannels, interval * 8, interval, false);
    latencySamples = impl->stretch.inputLatency() + impl->stretch.outputLatency() + quantum;
    ratioCoefficient = -std::expm1(-static_cast<float>(quantum / (sampleRate * 0.003)));
    prepared = true;
    reset();
}

void PitchShifter::reset()
{
    if (prepared)
    {
        impl->stretch.reset();
        impl->stretch.setTransposeFactor(1.0f);
    }
    for (auto& channel : input) channel.fill(0.0f);
    for (auto& channel : output) channel.fill(0.0f);
    fifoPosition = 0;
    smoothedSemitones = 0.0f;
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer, float targetPitchRatio,
                           float /*pitchPeriodSamples*/, bool isVoiced)
{
    if (!prepared || buffer.getNumSamples() <= 0)
        return;
    const int channels = std::min(numChannels, buffer.getNumChannels());
    if (channels <= 0)
        return;
    targetPitchRatio = std::isfinite(targetPitchRatio)
        ? juce::jlimit(0.5f, 2.0f, targetPitchRatio) : 1.0f;
    const float targetSemitones = isVoiced ? 12.0f * std::log2(targetPitchRatio) : 0.0f;

    // A persistent FIFO makes DSP and control updates independent of callback
    // partitioning. Read the previous output BEFORE submitting the next quantum.
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float sample = ch < channels ? buffer.getSample(ch, i) : 0.0f;
            input[static_cast<size_t>(ch)][static_cast<size_t>(fifoPosition)] =
                std::isfinite(sample) ? sample : 0.0f;
            if (ch < channels)
                buffer.setSample(ch, i, output[static_cast<size_t>(ch)][static_cast<size_t>(fifoPosition)]);
        }
        if (++fifoPosition == quantum)
        {
            fifoPosition = 0;
            smoothedSemitones += ratioCoefficient * (targetSemitones - smoothedSemitones);
            if (std::abs(smoothedSemitones - targetSemitones) < 1.0e-5f)
                smoothedSemitones = targetSemitones;
            impl->stretch.setTransposeSemitones(smoothedSemitones);
            const float* inputs[2] { input[0].data(), input[1].data() };
            float* outputs[2] { output[0].data(), output[1].data() };
            impl->stretch.process(inputs, quantum, outputs, quantum);
        }
    }
}

} // namespace VocalTune
