#include "AutoTuneEngine.h"
#include "DSPBenchmark.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {
thread_local bool counting = false;
thread_local size_t allocations = 0;
void recordAllocation() { if (counting) ++allocations; }
}

void* operator new(std::size_t n)
{
    recordAllocation();
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t alignment)
{
    recordAllocation();
    const auto a = static_cast<size_t>(alignment);
#if defined(_MSC_VER)
    void* p = _aligned_malloc(n == 0 ? 1 : n, a);
#else
    void* p = std::aligned_alloc(a, ((n + a - 1) / a + (n == 0)) * a);
#endif
    if (p) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--benchmark") return runDSPBenchmark();
    juce::ScopedNoDenormals noDenormals;
    bool passed = true;
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (bool lowLatency : { false, true })
        {
            VocalTune::AutoTuneEngine engine;
            engine.setLowLatencyMode(lowLatency);
            engine.prepare(rate, 64, 2);
            // Allocate the host buffer before counting, including space for an
            // oversized callback. The first DSP call is counted as well.
            juce::AudioBuffer<float> block(2, 4097);
            float* pointers[2] { block.getWritePointer(0), block.getWritePointer(1) };
            allocations = 0;
            double processingSeconds = 0.0;
            int position = 0;
            int callback = 0;
            while (position < static_cast<int>(rate * 2.0))
            {
                const int count = callback % 97 == 0 ? 4097 : 64;
                for (int i = 0; i < count; ++i)
                {
                    const double time = (position + i) / rate;
                    const bool silent = time > 0.7 && time < 1.0;
                    const auto sample = silent ? 0.0f : static_cast<float>(
                        0.2 * std::sin(6.283185307179586 * 207.0 * time)
                        + 0.07 * std::sin(6.283185307179586 * 414.0 * time));
                    pointers[0][i] = sample;
                    pointers[1][i] = -0.5f * sample;
                }
                juce::AudioBuffer<float> view(pointers, 2, count);
                const auto begin = std::chrono::steady_clock::now();
                counting = true;
                engine.process(view, callback % 13 == 0 ? 0.0f : 100.0f, 15.0f, 0.0f,
                               40.0f, static_cast<float>(callback % 25 - 12),
                               (callback / 40) % 2 == 0 ? 0.0f : 40.0f, callback % 6,
                               callback % 12, VocalTune::ScaleType::Major, 440.0f, 3, 0, true, 0x0fff);
                counting = false;
                processingSeconds += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - begin).count();
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < count; ++i)
                        passed = passed && std::isfinite(pointers[ch][i]);
                position += count;
                ++callback;
            }
            std::cout << rate << " Hz, low latency " << lowLatency
                      << ": callback C++ allocations=" << allocations
                      << ", processing/audio=" << processingSeconds / (position / rate) << '\n';
            passed = passed && allocations == 0;
        }
    return passed ? 0 : 1;
}
