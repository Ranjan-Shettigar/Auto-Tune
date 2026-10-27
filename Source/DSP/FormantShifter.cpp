#include "FormantShifter.h"
#include <cmath>
#include <algorithm>

namespace VocalTune
{

FormantShifter::FormantShifter() = default;

void FormantShifter::prepare(double sampleRate, int /*maxBlockSize*/, int channels)
{
    currentSampleRate = (sampleRate > 8000.0) ? sampleRate : 48000.0;
    numChannels = std::max(1, channels);
    smoothingCoefficient = -std::expm1(-coefficientInterval / (0.025 * currentSampleRate));
    filterStates.resize(static_cast<size_t>(numChannels));
    reset();
}

void FormantShifter::reset()
{
    for (auto& channel : filterStates)
        for (auto& state : channel)
            state = {};
    for (int b = 0; b < NumBands; ++b)
    {
        coefficients[b] = { 1.0, 0.0, 0.0, 0.0, 0.0 };
        targetCoefficients[b] = coefficients[b];
        coefficientSteps[b] = {};
    }
    smoothedShift = 0.0;
    coefficientShift = 0.0;
    rampActive = false;
    neutral = true;
    samplesUntilCoefficientUpdate = 0;
}

void FormantShifter::updateFilters(double shiftSemitones)
{
    smoothedShift += smoothingCoefficient * (shiftSemitones - smoothedShift);
    if (std::abs(smoothedShift - shiftSemitones) < 1.0e-6)
        smoothedShift = shiftSemitones;
    if (smoothedShift == coefficientShift)
        return;
    coefficientShift = smoothedShift;
    rampActive = true;
    neutral = false;
    const double shiftRatio = std::exp2(smoothedShift / 12.0);
    // Fade the resonances continuously through neutral. Previously the entire
    // effect and its frozen filter state switched abruptly at 0.05 semitone.
    const double gainAmount = std::min(1.0, std::abs(smoothedShift));

    for (int b = 0; b < NumBands; ++b)
    {
        Coefficients next { 1.0, 0.0, 0.0, 0.0, 0.0 };
        if (gainAmount > 0.0)
        {
            const double frequency = juce::jlimit(100.0, currentSampleRate * 0.49,
                                                   baseBands[b].baseFreq * shiftRatio);
            const double omega = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
            const double a = std::pow(10.0, baseBands[b].gainDb * gainAmount / 40.0);
            const double bandwidth = std::sin(omega) / (2.0 * baseBands[b].q);
            const double cosine = std::cos(omega);
            const double normalisation = 1.0 / (1.0 + bandwidth / a);
            next = { (1.0 + bandwidth * a) * normalisation,
                     -2.0 * cosine * normalisation,
                     (1.0 - bandwidth * a) * normalisation,
                     -2.0 * cosine * normalisation,
                     (1.0 - bandwidth / a) * normalisation };
        }
        targetCoefficients[b] = next;
        for (size_t k = 0; k < next.size(); ++k)
            coefficientSteps[b][k] = (next[k] - coefficients[b][k]) / coefficientInterval;
    }
}

void FormantShifter::process(juce::AudioBuffer<float>& buffer, float formantShiftSemitones)
{
    const double targetShift = std::isfinite(formantShiftSemitones)
        ? juce::jlimit(-12.0, 12.0, static_cast<double>(formantShiftSemitones)) : 0.0;
    const int totalChannels = std::min(numChannels, buffer.getNumChannels());
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (samplesUntilCoefficientUpdate == 0)
        {
            updateFilters(targetShift);
            samplesUntilCoefficientUpdate = coefficientInterval;
        }
        if (neutral && targetShift == 0.0)
        {
            // Keep the control clock running across host blocks even while idle.
            const int remaining = buffer.getNumSamples() - i;
            const int phase = (remaining - 1) % coefficientInterval + 1;
            samplesUntilCoefficientUpdate = (samplesUntilCoefficientUpdate - phase
                                              + coefficientInterval) % coefficientInterval;
            return;
        }
        if (rampActive)
            for (int b = 0; b < NumBands; ++b)
                for (size_t k = 0; k < coefficients[b].size(); ++k)
                    coefficients[b][k] += coefficientSteps[b][k];

        for (int ch = 0; ch < totalChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            double sample = data[i];
            for (int b = 0; b < NumBands; ++b)
            {
                const auto& c = coefficients[b];
                auto& state = filterStates[static_cast<size_t>(ch)][b];
                const double output = c[0] * sample + state.z1;
                state.z1 = c[1] * sample - c[3] * output + state.z2;
                state.z2 = c[2] * sample - c[4] * output;
                sample = output;
            }
            data[i] = static_cast<float>(sample);
        }
        if (--samplesUntilCoefficientUpdate == 0)
        {
            coefficients = targetCoefficients;
            rampActive = false;
            // Bypass only exact identity with fully drained delay states. This
            // preserves the tail and avoids replaying frozen state on reactivation.
            neutral = coefficientShift == 0.0;
            if (neutral)
                for (const auto& channel : filterStates)
                    for (const auto& state : channel)
                        neutral = neutral && state.z1 == 0.0 && state.z2 == 0.0;
        }
    }
}

} // namespace VocalTune
