#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

namespace VocalTune
{

class FormantShifter
{
public:
    FormantShifter();
    ~FormantShifter() = default;

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // formantShiftSemitones: -12.0 to +12.0 st (0.0 = neutral)
    void process(juce::AudioBuffer<float>& buffer, float formantShiftSemitones);

private:
    double currentSampleRate = 48000.0;
    int numChannels = 2;

    // Resonant formant filters for typical vocal tract formants (F1, F2, F3)
    struct FormantBand
    {
        float baseFreq;
        float q;
        float gainDb;
    };

    static constexpr int NumBands = 3;
    FormantBand baseBands[NumBands] = {
        { 600.0f,  2.5f, 4.0f },  // F1
        { 1600.0f, 3.0f, 3.5f },  // F2
        { 2800.0f, 3.5f, 3.0f }   // F3
    };

    // Normalised biquad coefficients: b0, b1, b2, a1, a2. These fixed arrays
    // avoid allocating reference-counted coefficient objects on the audio thread.
    using Coefficients = std::array<double, 5>;
    struct FilterState { double z1 = 0.0, z2 = 0.0; };
    std::vector<std::array<FilterState, NumBands>> filterStates;
    std::array<Coefficients, NumBands> coefficients {};
    std::array<Coefficients, NumBands> coefficientSteps {};
    std::array<Coefficients, NumBands> targetCoefficients {};
    static constexpr int coefficientInterval = 32;
    int samplesUntilCoefficientUpdate = 0;
    double smoothedShift = 0.0;

    void updateFilters(double shiftSemitones);
};

} // namespace VocalTune

