Run the native regressions with the same compiler and JUCE checkout as the plugin:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --target VocalTuneDSPTests VocalTuneEngineTests VocalTuneRealtimeTests --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

The pitch shifter checks both latency modes at 44.1, 48, and 96 kHz. It measures
impulse delay against the host-reported latency, delayed unity noise preservation,
ring-wrap behavior, stereo phase/gain coherence, shifted sine tuning and spectral
purity, short-window level dropouts, block-size independence, and mode changes
between prepared sessions. Selecting an already-active mode must preserve history.
Unity, transposed, and unvoiced transitions are checked for clicks and lost audio.

The engine checks pitch correction, vibrato, and formant processing together over
different host block partitions, including blocks larger than the prepared maximum.
Separate formant checks cover neutral transparency and automated transitions through
zero and both octave extremes.
Pitch detection is checked at 55, 82.4, 220, 880, and 1100 Hz through 192 kHz sample
rates, including DC rejection and pitch reacquisition after silence. Decimation
checks reject high-frequency tones that would alias onto 220 Hz, and preserve a
220 Hz vocal beneath those louder tones. Every decimation stage is exercised.
Formant automation includes sustained settings long enough to settle, drain tails,
enter neutral bypass, and reactivate with different host block partitions.

The real-time suite counts C++ `new`/aligned `new` calls during processing, including
the first callback, silence recovery, parameter changes and oversized buffers at
44.1, 48, 96 and 192 kHz. It also reports average processing time relative to audio
duration. This is an allocation regression, not a hard real-time deadline guarantee
or a hook for every platform C allocator.

For a separate component CPU profile (not a pass/fail timing test):

```powershell
./build/Release/VocalTuneRealtimeTests.exe --benchmark > build/cpu-profile.csv
```

This profiles the detector, shifter, neutral/active formant filters, full engine,
and silent engine at 48/96/192 kHz in both modes. It uses stereo 64-sample blocks
and excludes preparation, input copies and the first second of audio. Output
includes elapsed DSP time/audio duration, callback p99/max timings and Windows
thread cycles/sample. UI rendering is excluded. Repeat on the same machine;
scheduling, CPU frequency and other applications affect the results. See
[CPU optimization](../docs/CPU_OPTIMIZATION.md) for the before/after measurements.

Release validation on 2026-10-27: all three CTest suites passed. The VST3 also
passed pluginval v0.2.7, strictness level 5, with GUI tests disabled. Its tests cover
scanning, loading, processing, automation, state and bus layouts. Editor interaction
and recorded-vocal listening were not validated by that headless run.

These deterministic checks catch timing, discontinuity, and spectral regressions.
They do not establish perceptual equivalence to a commercial product. Listening
tests on recorded vocals and validation in the target DAW remain necessary.
