Run the native regressions with the same compiler and JUCE checkout as the plugin:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --target VocalTuneDSPTests VocalTuneEngineTests --parallel 4
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
Pitch detection is checked at 55, 82.4, 220, and 880 Hz through 192 kHz sample rates,
including DC rejection and pitch reacquisition after silence.

These deterministic checks catch timing, discontinuity, and spectral regressions.
They do not establish perceptual equivalence to a commercial product. Listening
tests on recorded vocals and validation in the target DAW remain necessary.
