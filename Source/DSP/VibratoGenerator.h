#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace VocalTune
{

class VibratoGenerator
{
public:
    VibratoGenerator() = default;

    void prepare(double sampleRate)
    {
        currentSampleRate = (sampleRate > 8000.0) ? sampleRate : 48000.0;
        reset();
    }

    void reset()
    {
        phase = 0.0f;
        smoothedDepth = 0.0f;
    }

    // Called by the engine's fixed control clock, independent of host blocks.
    // vibratoPercent: 0.0 to 100.0 %
    float getNextRatio(float vibratoPercent, int numSamples)
    {
        if (numSamples <= 0)
            return 1.0f;

        const float targetDepth = std::isfinite(vibratoPercent)
            ? juce::jlimit(0.0f, 1.0f, vibratoPercent * 0.01f) : 0.0f;
        const float depthAlpha = -std::expm1(-static_cast<float>(numSamples)
                                            / (0.020f * static_cast<float>(currentSampleRate)));
        smoothedDepth += depthAlpha * (targetDepth - smoothedDepth);

        // Vibrato rate ~ 5.5 Hz
        const float rateHz = 5.5f;
        const double phaseInc = (rateHz / currentSampleRate) * numSamples;
        phase += phaseInc;
        if (phase >= 1.0)
            phase -= std::floor(phase);

        // Sine vibrato with subtle warmth
        const float lfo = static_cast<float>(std::sin(phase * juce::MathConstants<double>::twoPi));

        // Max depth at 100% vibrato is approx +/- 0.5 semitone (+/- 50 cents)
        float maxSemitones = 0.5f;
        float semitoneOffset = lfo * smoothedDepth * maxSemitones;

        return std::pow(2.0f, semitoneOffset / 12.0f);
    }

private:
    double currentSampleRate = 48000.0;
    double phase = 0.0;
    float smoothedDepth = 0.0f;
};

} // namespace VocalTune

