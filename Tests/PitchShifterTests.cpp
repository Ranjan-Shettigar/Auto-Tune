#include "PitchShifter.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846;

struct Checks
{
    int failures = 0;

    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }
};

using Samples = std::vector<float>;

struct Render
{
    Samples left;
    Samples right;
    int latency = 0;
};

Render render(const Samples& input, double sampleRate, bool lowLatency, float ratio,
              float period, const std::vector<int>& blocks, bool repeatMode = false)
{
    VocalTune::PitchShifter shifter;
    shifter.setLowLatencyMode(lowLatency);
    shifter.prepare(sampleRate, *std::max_element(blocks.begin(), blocks.end()), 2);
    Render result { Samples(input.size()), Samples(input.size()), shifter.getLatencySamples() };
    size_t position = 0;
    size_t blockIndex = 0;
    while (position < input.size())
    {
        const auto count = static_cast<int>(std::min<size_t>(
            static_cast<size_t>(blocks[blockIndex % blocks.size()]), input.size() - position));
        juce::AudioBuffer<float> block(2, count);
        for (int n = 0; n < count; ++n)
        {
            block.setSample(0, n, input[position + static_cast<size_t>(n)]);
            block.setSample(1, n, -0.5f * input[position + static_cast<size_t>(n)]);
        }
        if (repeatMode && position >= input.size() / 2 &&
            position - static_cast<size_t>(count) < input.size() / 2)
            shifter.setLowLatencyMode(lowLatency);
        shifter.process(block, ratio, period, true);
        for (int n = 0; n < count; ++n)
        {
            result.left[position + static_cast<size_t>(n)] = block.getSample(0, n);
            result.right[position + static_cast<size_t>(n)] = block.getSample(1, n);
        }
        position += static_cast<size_t>(count);
        ++blockIndex;
    }
    return result;
}

Samples sine(double sampleRate, double frequency, double seconds)
{
    Samples result(static_cast<size_t>(sampleRate * seconds));
    for (size_t n = 0; n < result.size(); ++n)
        result[n] = 0.5f * static_cast<float>(std::sin(2.0 * pi * frequency * static_cast<double>(n) / sampleRate));
    return result;
}

double rms(const Samples& input, size_t begin, size_t end)
{
    double energy = 0.0;
    for (auto n = begin; n < end; ++n)
        energy += static_cast<double>(input[n]) * input[n];
    return std::sqrt(energy / static_cast<double>(end - begin));
}

double maxDifference(const Samples& first, const Samples& second)
{
    double result = 0.0;
    for (size_t n = 0; n < first.size(); ++n)
        result = std::max(result, std::abs(static_cast<double>(first[n]) - second[n]));
    return result;
}

// A small offline FFT keeps the regression runner independent of the plugin UI
// and measures frequency, sidebands and gain rather than checking implementation details.
void fft(std::vector<std::complex<double>>& values)
{
    const auto size = values.size();
    for (size_t i = 1, j = 0; i < size; ++i)
    {
        auto bit = size >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(values[i], values[j]);
    }
    for (size_t length = 2; length <= size; length <<= 1)
    {
        const auto step = std::polar(1.0, -2.0 * pi / static_cast<double>(length));
        for (size_t start = 0; start < size; start += length)
        {
            std::complex<double> phase(1.0, 0.0);
            for (size_t j = 0; j < length / 2; ++j)
            {
                const auto even = values[start + j];
                const auto odd = values[start + j + length / 2] * phase;
                values[start + j] = even + odd;
                values[start + j + length / 2] = even - odd;
                phase *= step;
            }
        }
    }
}

struct Spectrum
{
    double frequency = 0.0;
    double tonalEnergy = 0.0;
};

Spectrum measureSpectrum(const Samples& input, double sampleRate, double expectedFrequency)
{
    size_t size = 1;
    while (size * 2 <= input.size() / 2)
        size *= 2;
    std::vector<std::complex<double>> bins(size);
    for (size_t n = 0; n < size; ++n)
    {
        const auto window = 0.5 - 0.5 * std::cos(2.0 * pi * static_cast<double>(n) / static_cast<double>(size));
        bins[n] = static_cast<double>(input[input.size() - size + n]) * window;
    }
    fft(bins);
    const auto binHz = sampleRate / static_cast<double>(size);
    const auto start = static_cast<size_t>(std::max(2.0, std::floor(expectedFrequency * 0.7 / binHz)));
    const auto end = std::min(size / 2 - 2,
                              static_cast<size_t>(std::ceil(expectedFrequency * 1.3 / binHz)));
    auto peak = start;
    double totalEnergy = 0.0;
    for (size_t n = 1; n < size / 2; ++n)
    {
        totalEnergy += std::norm(bins[n]);
        if (n >= start && n <= end && std::norm(bins[n]) > std::norm(bins[peak]))
            peak = n;
    }
    const auto a = std::log(std::max(1.0e-30, std::abs(bins[peak - 1])));
    const auto b = std::log(std::max(1.0e-30, std::abs(bins[peak])));
    const auto c = std::log(std::max(1.0e-30, std::abs(bins[peak + 1])));
    const auto denominator = a - 2.0 * b + c;
    const auto offset = std::abs(denominator) > 1.0e-15 ? 0.5 * (a - c) / denominator : 0.0;
    double fundamentalEnergy = 0.0;
    for (auto n = peak - std::min<size_t>(peak, 3); n <= peak + 3; ++n)
        fundamentalEnergy += std::norm(bins[n]);
    return { (static_cast<double>(peak) + offset) * binHz,
             totalEnergy > 1.0e-20 ? fundamentalEnergy / totalEnergy : 0.0 };
}

std::string label(double sampleRate, bool lowLatency)
{
    return std::to_string(static_cast<int>(sampleRate)) + " Hz, low latency " + (lowLatency ? "ON" : "OFF");
}

void checkFiniteStereo(Checks& checks, const Render& audio, const std::string& name)
{
    double stereoError = 0.0;
    bool finite = true;
    for (size_t n = 0; n < audio.left.size(); ++n)
    {
        finite = finite && std::isfinite(audio.left[n]) && std::isfinite(audio.right[n]);
        stereoError = std::max(stereoError, std::abs(static_cast<double>(audio.right[n]) + 0.5 * audio.left[n]));
    }
    checks.require(finite, name + ": finite samples");
    if (stereoError >= 2.0e-6)
        std::cout << "  stereo max error=" << stereoError << '\n';
    if (stereoError >= 2.0e-6)
        std::cout << "  stereo max error=" << stereoError << '\n';
    checks.require(stereoError < 2.0e-6, name + ": stereo gain/phase coherence");
}

void checkIdentity(Checks& checks, double sampleRate, bool lowLatency)
{
    const auto name = label(sampleRate, lowLatency);
    Samples input(static_cast<size_t>(sampleRate * 2.5));
    uint32_t random = 0x87654321u;
    for (auto& sample : input)
    {
        random = random * 1664525u + 1013904223u;
        sample = 0.4f * (static_cast<float>(random >> 8) / 8388608.0f - 1.0f);
    }
    const auto output = render(input, sampleRate, lowLatency, 1.0f, 0.0f, { 127, 512, 31, 1024 });
    Samples expected(input.size(), 0.0f);
    for (size_t n = static_cast<size_t>(output.latency); n < input.size(); ++n)
        expected[n] = input[n - static_cast<size_t>(output.latency)];
    const auto error = maxDifference(output.left, expected);
    std::cout << name << ": declared latency=" << output.latency << ", identity max error=" << error << '\n';
    checks.require(error < 2.0e-6, name + ": unity must equal input delayed by reported latency, including ring wraps");
    checkFiniteStereo(checks, output, name);

    Samples impulses(input.size(), 0.0f);
    const size_t impulsePosition = static_cast<size_t>(sampleRate * 0.2);
    impulses[impulsePosition] = 1.0f;
    const auto impulseOutput = render(impulses, sampleRate, lowLatency, 1.0f, 0.0f, { 257 });
    const auto peak = static_cast<size_t>(std::distance(impulseOutput.left.begin(),
        std::max_element(impulseOutput.left.begin(), impulseOutput.left.end())));
    std::cout << "  measured impulse latency=" << static_cast<int64_t>(peak) - static_cast<int64_t>(impulsePosition) << '\n';
    checks.require(peak == impulsePosition + static_cast<size_t>(output.latency), name + ": impulse agrees with host latency");

    const auto repeatedMode = render(input, sampleRate, lowLatency, 1.0f, 0.0f, { 256 }, true);
    checks.require(maxDifference(repeatedMode.left, expected) < 2.0e-6,
                   name + ": selecting the active mode preserves delay history");
}

void checkShift(Checks& checks, double sampleRate, bool lowLatency, double frequency, float ratio)
{
    const auto name = label(sampleRate, lowLatency) + ", " + std::to_string(frequency) + " Hz x " + std::to_string(ratio);
    const auto input = sine(sampleRate, frequency, 3.0);
    const auto output = render(input, sampleRate, lowLatency, ratio,
                               static_cast<float>(sampleRate / frequency), { 64, 511, 127, 1024 });
    checkFiniteStereo(checks, output, name);
    const auto spectrum = measureSpectrum(output.left, sampleRate, frequency * ratio);
    const auto cents = 1200.0 * std::log2(spectrum.frequency / (frequency * ratio));
    const auto steadyStart = static_cast<size_t>(sampleRate);
    const auto gain = rms(output.left, steadyStart, output.left.size()) / rms(input, steadyStart, input.size());
    const auto envelopeLength = static_cast<size_t>(std::ceil(4.0 * sampleRate / (frequency * ratio)));
    double minGain = std::numeric_limits<double>::max();
    double maxGain = 0.0;
    for (size_t n = steadyStart; n + envelopeLength < output.left.size(); n += envelopeLength / 2)
    {
        const auto localGain = rms(output.left, n, n + envelopeLength) / (0.5 / std::sqrt(2.0));
        minGain = std::min(minGain, localGain);
        maxGain = std::max(maxGain, localGain);
    }
    std::cout << name << ": cents=" << cents << ", gain=" << gain
              << ", tonal energy=" << spectrum.tonalEnergy << ", envelope=" << minGain << ".." << maxGain << '\n';
    checks.require(std::abs(cents) < 7.0, name + ": pitch accuracy within 7 cents");
    checks.require(gain > 0.85 && gain < 1.15, name + ": stable shifted gain");
    checks.require(spectrum.tonalEnergy > 0.90, name + ": at least 90% energy in requested fundamental");
    checks.require(minGain > 0.70 && maxGain < 1.30, name + ": no periodic dropouts or gain bursts");

    if (frequency == 220.0)
    {
        const auto partitioned = render(input, sampleRate, lowLatency, ratio,
                                        static_cast<float>(sampleRate / frequency), { 1, 73, 256, 997 });
        checks.require(maxDifference(partitioned.left, output.left) < 2.0e-6,
                       name + ": rendering does not depend on host block boundaries");
    }
}

void checkModeReprepare(Checks& checks)
{
    VocalTune::PitchShifter shifter;
    for (const bool mode : { false, true, false, true })
    {
        shifter.setLowLatencyMode(mode);
        shifter.prepare(48000.0, 257, 2);
        const auto latency = shifter.getLatencySamples();
        int peak = -1;
        for (int position = 0; position < latency + 1024; position += 257)
        {
            juce::AudioBuffer<float> block(2, 257);
            block.clear();
            if (position == 0)
                block.setSample(0, 0, 1.0f);
            shifter.process(block, 1.0f, 0.0f, true);
            for (int n = 0; n < 257; ++n)
                if (block.getSample(0, n) > 0.99f)
                    peak = position + n;
        }
        checks.require(peak == latency, "mode switch followed by prepare: reported impulse latency");
    }
}

void checkTransitions(Checks& checks, bool lowLatency)
{
    constexpr double sampleRate = 48000.0;
    const auto input = sine(sampleRate, 220.0, 4.0);
    Samples output(input.size());
    VocalTune::PitchShifter shifter;
    shifter.setLowLatencyMode(lowLatency);
    shifter.prepare(sampleRate, 64, 1);
    for (size_t position = 0; position < input.size(); position += 64)
    {
        const int count = static_cast<int>(std::min<size_t>(64, input.size() - position));
        juce::AudioBuffer<float> block(1, count);
        block.copyFrom(0, 0, input.data() + position, count);
        const auto second = static_cast<int>(position / 48000);
        const float ratio = second == 1 || second == 2 ? std::pow(2.0f, 1.0f / 12.0f) : 1.0f;
        const bool voiced = second != 2;
        shifter.process(block, ratio, static_cast<float>(sampleRate / 220.0), voiced);
        std::copy_n(block.getReadPointer(0), count, output.data() + position);
    }
    double maximumStep = 0.0;
    double minimumRms = 1.0;
    bool finite = true;
    for (size_t n = 24000; n < output.size(); ++n)
    {
        finite = finite && std::isfinite(output[n]);
        maximumStep = std::max(maximumStep, std::abs(static_cast<double>(output[n]) - output[n - 1]));
    }
    for (size_t n = 24000; n + 960 < output.size(); n += 240)
        minimumRms = std::min(minimumRms, rms(output, n, n + 960));
    const auto name = label(sampleRate, lowLatency);
    std::cout << name << ": unity/shift/unvoiced transitions max step=" << maximumStep
              << ", minimum 20 ms RMS=" << minimumRms << '\n';
    checks.require(finite && maximumStep < 0.15, name + ": no clicks across unity and voicing changes");
    checks.require(minimumRms > 0.05, name + ": continuous audio across unity and voicing changes");
}
}

int main()
{
    Checks checks;
    std::cout << std::fixed << std::setprecision(6);
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        for (const bool lowLatency : { false, true })
        {
            checkIdentity(checks, sampleRate, lowLatency);
            checkShift(checks, sampleRate, lowLatency, 110.0, std::pow(2.0f, 2.0f / 12.0f));
            checkShift(checks, sampleRate, lowLatency, 220.0, std::pow(2.0f, -2.0f / 12.0f));
            checkShift(checks, sampleRate, lowLatency, 440.0, 2.0f);
            checkShift(checks, sampleRate, lowLatency, 110.0, 0.5f);
        }
    }
    checkModeReprepare(checks);
    checkTransitions(checks, false);
    checkTransitions(checks, true);
    std::cout << "DSP regression failures: " << checks.failures << '\n';
    return checks.failures == 0 ? 0 : 1;
}
