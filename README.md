# Vocal Tune

A C++20/JUCE 8 monophonic vocal pitch-correction plugin for Windows, built as VST3
and standalone. Select a root and scale, then adjust correction, vibrato, and vocal
resonance colour. Session parameters retain their existing IDs.

## Artifact fixes

The previous shared shifter read unwritten input, wrote overlap data into already
consumed output slots, searched an already-cleared correlation reference, and
switched abruptly between incompatible unity and shifted paths. Its measured dry
delay was twice the delay reported to the host. Both latency modes were affected.

The replacement uses continuous stereo-linked spectral processing based on
[Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch),
with local short-window tuning and stereo fixes. Pitch analysis scales with sample
rate; smoothing and vibrato use a persistent control clock. DSP buffers and filters
are allocated before processing, including support for oversized host buffers.
See [the investigation](docs/ARTIFACT_FIX.md) and [vendor notices](ThirdParty/README.md).

## Latency modes

| Sample rate | Low Latency ON | Low Latency OFF |
| --- | --- | --- |
| 44.1 kHz | 1796 samples / 40.73 ms | 3560 samples / 80.73 ms |
| 48 kHz | 1952 samples / 40.67 ms | 3872 samples / 80.67 ms |
| 96 kHz | 3872 samples / 40.33 ms | 7712 samples / 80.33 ms |

These are algorithmic audio delays, excluding the audio-interface/DAW buffer.
The host receives the actual delay for compensation. Pitch acquisition additionally
needs a voiced analysis window; the old 2.6 ms claim described an analysis hop, not
usable end-to-end tuning latency. ON uses a shorter window; OFF gives more spectral
context. This implementation is not an ultra-low-latency live-monitoring engine.

Changing modes while audio is prepared shows **Audio restart required**. Restart the
DAW audio device or reload the plugin after saving the setting. Stopping transport
alone may not reprepare a plugin. This avoids clearing live audio or changing host
compensation inside the audio callback.

## Controls and limits

- **Correction:** correction depth and retune speed; 0 disables correction, 100 uses
  a short smoothing ramp for fast tuning.
- **Vibrato:** 5.5 Hz modulation, up to approximately +/-50 cents.
- **Formant Shift:** smooth three-band resonance-colour effect. It is an EQ
  approximation, not true spectral-envelope formant preservation.
- **Root, Scale, Follow, Custom notes:** select allowed target notes.
- **Tuning:** reference A4 from 415 to 465 Hz.

Use isolated, monophonic vocals. Automated signal tests cannot establish perceptual
parity with commercial tuning plugins; recorded-vocal listening tests in the target
DAW are still needed, especially for breaths, transients, and extreme correction.

## Build and test

Requires CMake 3.22+, Visual Studio 2022 C++ tools, and JUCE 8. Set
`VOCALTUNE_JUCE_PATH` to a JUCE checkout to build offline. Otherwise CMake can fetch
JUCE 8.0.6. The pitch backend is vendored with its MIT notices.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Outputs:

- `build/VocalTune_artefacts/Release/VST3/Vocal Tune.vst3`
- `build/VocalTune_artefacts/Release/Standalone/Vocal Tune.exe`

Copy the complete `.vst3` bundle to your chosen VST3 folder and rescan in the DAW.
The build does not overwrite an installed plugin automatically. Test coverage and
limitations are described in [Tests/README.md](Tests/README.md).
