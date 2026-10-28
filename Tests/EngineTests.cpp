#include "AutoTuneEngine.h"
#include "FormantShifter.h"
#include "PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double sampleRate = 48000.0;
using Samples = std::vector<float>;

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

Samples vocalSignal()
{
    Samples result(static_cast<size_t>(sampleRate * 2.0));
    double phase = 0.0;
    for (size_t n = 0; n < result.size(); ++n)
    {
        const double time = static_cast<double>(n) / sampleRate;
        const double frequency = time < 0.7 ? 213.0 : (time < 1.4 ? 287.0 : 163.0);
        phase += 2.0 * pi * frequency / sampleRate;
        result[n] = static_cast<float>(0.20 * std::sin(phase) + 0.07 * std::sin(phase * 2.0)
                                       + 0.03 * std::sin(phase * 3.0));
        if (time > 1.0 && time < 1.1)
            result[n] = 0.0f;
    }
    return result;
}

Samples renderEngine(const Samples& input, bool lowLatency, const std::vector<int>& blockSizes)
{
    VocalTune::AutoTuneEngine engine;
    engine.setLowLatencyMode(lowLatency);
    // Deliberately process blocks above this advertised maximum as well.
    engine.prepare(sampleRate, 64, 2);
    Samples output(input.size());
    size_t position = 0;
    size_t blockIndex = 0;
    while (position < input.size())
    {
        const int count = static_cast<int>(std::min(input.size() - position,
            static_cast<size_t>(blockSizes[blockIndex++ % blockSizes.size()])));
        juce::AudioBuffer<float> block(2, count);
        for (int n = 0; n < count; ++n)
        {
            block.setSample(0, n, input[position + static_cast<size_t>(n)]);
            block.setSample(1, n, input[position + static_cast<size_t>(n)]);
        }
        engine.process(block, 80.0f, 15.0f, 10.0f, 35.0f, 2.0f, 25.0f, 1, 0, VocalTune::ScaleType::Major,
                       440.0f, 3, 0, true, 0x0fff);
        for (int n = 0; n < count; ++n)
            output[position + static_cast<size_t>(n)] = block.getSample(0, n);
        position += static_cast<size_t>(count);
    }
    return output;
}

double difference(const Samples& first, const Samples& second)
{
    double maximum = 0.0;
    for (size_t n = 0; n < first.size(); ++n)
    {
        if (!std::isfinite(first[n]) || !std::isfinite(second[n]))
            return 1.0e20;
        maximum = std::max(maximum, std::abs(static_cast<double>(first[n]) - second[n]));
    }
    return maximum;
}

void checkPartitioning(Checks& checks)
{
    const auto input = vocalSignal();
    for (const bool lowLatency : { false, true })
    {
        const auto reference = renderEngine(input, lowLatency, { 32 });
        for (const auto& sizes : { std::vector<int> { 127 }, std::vector<int> { 512 },
                                  std::vector<int> { 1, 73, 4097, 256 } })
        {
            const auto candidate = renderEngine(input, lowLatency, sizes);
            const auto error = difference(reference, candidate);
            std::cout << "Engine low latency " << (lowLatency ? "ON" : "OFF")
                      << ", block " << sizes.front() << ": max partition error=" << error << '\n';
            checks.require(error < 3.0e-6, "engine tuning/vibrato/formant independent of host block sizes");
        }
    }
}

Samples renderFormants(const Samples& input, const std::vector<int>& blockSizes, bool automate)
{
    VocalTune::FormantShifter formant;
    formant.prepare(sampleRate, 64, 1);
    const float shifts[] { 0.0f, 12.0f, 0.0f, -12.0f, 0.0f };
    const size_t sectionLength = input.size() / 5;
    Samples output(input.size());
    size_t position = 0;
    size_t blockIndex = 0;
    while (position < input.size())
    {
        const auto section = std::min<size_t>(4, position / sectionLength);
        const auto boundary = section == 4 ? input.size() : (section + 1) * sectionLength;
        const int count = static_cast<int>(std::min(boundary - position,
            static_cast<size_t>(blockSizes[blockIndex++ % blockSizes.size()])));
        juce::AudioBuffer<float> block(1, count);
        block.copyFrom(0, 0, input.data() + position, count);
        formant.process(block, automate ? shifts[section] : 0.0f);
        std::copy_n(block.getReadPointer(0), count, output.data() + position);
        position += static_cast<size_t>(count);
    }
    return output;
}

void checkFormants(Checks& checks)
{
    auto input = vocalSignal();
    const auto phrase = input;
    input.insert(input.end(), phrase.begin(), phrase.end());
    input.insert(input.end(), phrase.begin(), phrase.end());
    const auto neutral = renderFormants(input, { 127 }, false);
    checks.require(difference(input, neutral) < 1.0e-7, "neutral formant processing preserves samples");
    const auto automated = renderFormants(input, { 32 }, true);
    const auto partitioned = renderFormants(input, { 7, 511, 4097 }, true);
    const auto error = difference(automated, partitioned);
    std::cout << "Formant automation max partition error=" << error << '\n';
    checks.require(error < 2.0e-6, "formant parameter changes independent of host blocks");
    double maximum = 0.0;
    double maxStep = 0.0;
    bool finite = true;
    for (size_t n = 0; n < automated.size(); ++n)
    {
        finite = finite && std::isfinite(automated[n]);
        maximum = std::max(maximum, std::abs(static_cast<double>(automated[n])));
        if (n > 0)
            maxStep = std::max(maxStep, std::abs(static_cast<double>(automated[n]) - automated[n - 1]));
    }
    std::cout << "Formant +/-12 semitone automation: peak=" << maximum << ", max step=" << maxStep << '\n';
    checks.require(finite && maximum < 1.0 && maxStep < 0.5,
                   "formant neutral/extreme transitions remain finite and bounded");
    double settledError = 0.0;
    for (size_t n = input.size() - static_cast<size_t>(sampleRate * 0.25); n < input.size(); ++n)
        settledError = std::max(settledError, std::abs(static_cast<double>(automated[n]) - input[n]));
    checks.require(settledError < 1.0e-7, "formant tail drains to transparent neutral after automation");
}

void checkDetector(Checks& checks)
{
    for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const bool lowLatency : { false, true })
        {
            VocalTune::PitchDetector detector;
            detector.setLowLatencyMode(lowLatency);
            detector.prepare(rate, 113);
            const auto feed = [&detector, rate](double frequency, double seconds, float dc)
            {
                int remaining = static_cast<int>(rate * seconds);
                int position = 0;
                while (remaining > 0)
                {
                    const int count = std::min(113, remaining);
                    float block[113] {};
                    for (int n = 0; n < count; ++n)
                    {
                        const auto phase = 2.0 * pi * frequency * (position + n) / rate;
                        block[n] = dc + (frequency > 0.0
                            ? static_cast<float>(0.20 * std::sin(phase) + 0.06 * std::sin(phase * 2.0)) : 0.0f);
                    }
                    VocalTune::PitchDetectionResult result;
                    detector.processBlock(block, count, result);
                    position += count;
                    remaining -= count;
                }
                return detector.getLatestResult();
            };
            for (const auto frequency : { 45.0, 55.0, 82.4, 220.0, 880.0, 1100.0, 1500.0 })
            {
                detector.reset();
                const auto result = feed(frequency, 0.25, 0.04f);
                const auto cents = result.pitchHz > 0.0f
                    ? 1200.0 * std::log2(result.pitchHz / frequency) : 1.0e20;
                std::cout << "Detector " << rate << " Hz, low latency " << (lowLatency ? "ON" : "OFF")
                          << ", input=" << frequency << ": cents=" << cents << '\n';
                checks.require(result.isVoiced && std::abs(cents) < 5.0,
                               "detector covers low/high vocal notes at every supported test rate");
            }
            const auto silence = feed(0.0, 0.15, 0.1f);
            checks.require(!silence.isVoiced, "DC and silence do not produce pitched notes");
            const auto restarted = feed(147.0, 0.08, 0.0f);
            const auto cents = restarted.pitchHz > 0.0f
                ? 1200.0 * std::log2(restarted.pitchHz / 147.0) : 1.0e20;
            checks.require(restarted.isVoiced && std::abs(cents) < 5.0,
                           "new phrase acquires its pitch without stale note history");
        }
    }
}

void checkHarmonyAutomation(Checks& checks)
{
    for (const bool lowLatency : { false, true })
    {
        auto render = [lowLatency](const std::vector<int>& sizes)
        {
            VocalTune::AutoTuneEngine engine;
            engine.setLowLatencyMode(lowLatency);
            engine.prepare(sampleRate, 64, 2);
            Samples output(static_cast<size_t>(sampleRate * 3));
            size_t offset = 0, blockIndex = 0;
            const size_t sectionSize = static_cast<size_t>(sampleRate / 2);
            while (offset < output.size())
            {
                const size_t section = offset / sectionSize;
                const int count = static_cast<int>(std::min({ output.size() - offset,
                    (section + 1) * sectionSize - offset,
                    static_cast<size_t>(sizes[blockIndex++ % sizes.size()]) }));
                juce::AudioBuffer<float> buffer(2, count);
                for (int n = 0; n < count; ++n)
                    for (int ch = 0; ch < 2; ++ch)
                        buffer.setSample(ch, n, static_cast<float>(0.2 * std::sin(2 * pi * 220 * (offset + n) / sampleRate)));
                const float mix = (section == 1 || section == 3) ? 60.0f : 0.0f;
                engine.process(buffer, 0, 0, 50, 0, 0, mix, 1, 0,
                    VocalTune::ScaleType::Chromatic, 440, 3, 0, false, 0xfff);
                std::copy_n(buffer.getReadPointer(0), count, output.data() + offset);
                offset += static_cast<size_t>(count);
            }
            return output;
        };
        const auto reference = render({ 32 });
        const auto partitioned = render({ 1, 73, 4097, 127 });
        checks.require(difference(reference, partitioned) < 3.0e-6,
                       "harmony warmup, fades and reactivation are independent of host blocks");
        double maxStep = 0;
        for (size_t n = 1; n < reference.size(); ++n)
            maxStep = std::max(maxStep, std::abs(static_cast<double>(reference[n]) - reference[n - 1]));
        std::cout << "Harmony automation mode " << lowLatency << ": max step=" << maxStep << '\n';
        checks.require(maxStep < 0.06, "harmony switching preserves continuous audio");
    }
}

void checkDetectorAliasing(Checks& checks)
{
    for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (const bool lowLatency : { false, true })
            for (const bool withVocal : { false, true })
            {
                VocalTune::PitchDetector detector;
                detector.setLowLatencyMode(lowLatency);
                detector.prepare(rate, 113);
                double analysisRate = rate;
                while (analysisRate > 12000.0) analysisRate *= 0.5;
                // Every decimation stage must reject energy which would fold
                // onto 220 Hz. Include the loud interferer with and without voice.
                for (double foldedRate = analysisRate; foldedRate < rate * 0.5; foldedRate *= 2.0)
                {
                    detector.reset();
                    for (int offset = 0; offset < static_cast<int>(rate * 0.25); offset += 113)
                    {
                        float block[113];
                        for (int n = 0; n < 113; ++n)
                        {
                            const double time = (offset + n) / rate;
                            block[n] = static_cast<float>(0.5 * std::sin(2 * pi * (foldedRate + 220.0) * time)
                                + (withVocal ? 0.2 * std::sin(2 * pi * 220.0 * time) : 0.0));
                        }
                        VocalTune::PitchDetectionResult result;
                        detector.processBlock(block, 113, result);
                    }
                    const auto result = detector.getLatestResult();
                    if (withVocal)
                        checks.require(result.isVoiced && result.pitchHz > 0.0f
                                       && std::abs(1200 * std::log2(result.pitchHz / 220.0)) < 5.0,
                                       "decimation preserves vocal pitch with a loud high-frequency interferer");
                    else
                        checks.require(!result.isVoiced, "decimation rejects false pitch from high-frequency aliases");
                }
            }
}
}

int main()
{
    Checks checks;
    checkPartitioning(checks);
    checkFormants(checks);
    checkDetector(checks);
    checkDetectorAliasing(checks);
    checkHarmonyAutomation(checks);
    std::cout << "Engine regression failures: " << checks.failures << '\n';
    return checks.failures == 0 ? 0 : 1;
}
