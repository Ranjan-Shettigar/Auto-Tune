#pragma once
#include "AutoTuneEngine.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Offline component profile. Input generation/copies and setup are untimed.
// Percent is elapsed DSP time / audio duration, not Windows total-CPU percent.
inline int runDSPBenchmark()
{
    juce::ScopedNoDenormals noDenormals;
    std::cout << "rate,low_latency,stage,block,processing_percent,p99_us,max_us,cycles_per_sample\n";
    for (double rate : { 48000.0, 96000.0, 192000.0 })
    {
        const int count = static_cast<int>(rate * 6.0);
        std::vector<float> signal(static_cast<size_t>(count));
        double phase = 0;
        for (int i = 0; i < count; ++i)
        {
            const auto time = i / rate;
            phase += juce::MathConstants<double>::twoPi * (207.0 + 10.0 * std::sin(time * 3.0)) / rate;
            signal[static_cast<size_t>(i)] = static_cast<float>(0.2 * std::sin(phase)
                + 0.07 * std::sin(2 * phase) + 0.03 * std::sin(3 * phase));
        }
        for (bool low : { false, true })
            for (const std::string stage : { "detector", "shifter", "formant_neutral", "formant_active", "engine", "silence" })
            {
                constexpr int blockSize = 64;
                VocalTune::AutoTuneEngine engine;
                VocalTune::PitchDetector detector;
                VocalTune::PitchShifter shifter;
                VocalTune::FormantShifter formant;
                engine.setLowLatencyMode(low);
                detector.setLowLatencyMode(low);
                shifter.setLowLatencyMode(low);
                engine.prepare(rate, blockSize, 2);
                detector.prepare(rate, blockSize);
                shifter.prepare(rate, blockSize, 2);
                formant.prepare(rate, blockSize, 2);
                juce::AudioBuffer<float> buffer(2, blockSize);
                std::vector<double> times;
                times.reserve(static_cast<size_t>(count / blockSize));
                double total = 0;
                unsigned long long totalCycles = 0;
                int measuredSamples = 0;
                for (int i = 0; i + blockSize <= count; i += blockSize)
                {
                    if (stage == "silence") buffer.clear();
                    else
                        for (int ch = 0; ch < 2; ++ch)
                            buffer.copyFrom(ch, 0, signal.data() + i, blockSize);
                    VocalTune::PitchDetectionResult detection;
#if JUCE_WINDOWS
                    ULONG64 beginCycles = 0, endCycles = 0;
                    QueryThreadCycleTime(GetCurrentThread(), &beginCycles);
#endif
                    const auto start = std::chrono::steady_clock::now();
                    if (stage == "detector") detector.processBlock(buffer.getReadPointer(0), blockSize, detection);
                    else if (stage == "shifter") shifter.process(buffer, 1.03f, static_cast<float>(rate / 207), true);
                    else if (stage == "formant_neutral") formant.process(buffer, 0);
                    else if (stage == "formant_active") formant.process(buffer, 2);
                    else engine.process(buffer, 80, 15, 0, 0, 0, 0, 0, 0, VocalTune::ScaleType::Major, 440, 0, 0, true, 0xfff);
                    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
#if JUCE_WINDOWS
                    QueryThreadCycleTime(GetCurrentThread(), &endCycles);
#endif
                    if (i >= static_cast<int>(rate))
                    {
                        total += seconds;
                        times.push_back(seconds * 1e6);
                        measuredSamples += blockSize;
#if JUCE_WINDOWS
                        totalCycles += endCycles - beginCycles;
#endif
                    }
                }
                std::sort(times.begin(), times.end());
                std::cout << std::fixed << std::setprecision(3) << rate << ',' << low << ',' << stage << ',' << blockSize
                    << ',' << 100.0 * total / (measuredSamples / rate)
                    << ',' << times[static_cast<size_t>(times.size() * 0.99)] << ',' << times.back()
                    << ',' << static_cast<double>(totalCycles) / measuredSamples << std::endl;
            }
    }
    return 0;
}
