# Vocal Tune - High-Quality Auto-Tune VST3 & Standalone Plugin

A real-time, low-latency pitch correction and vocal auto-tune audio plugin built with modern **C++20**, **JUCE 8**, and **CMake**, matching the sleek modern interface shown in the design specification.

---

## 📸 Interface & Controls Overview

- **Top Bar**:
  - **Vocal Tune Branding**: Stylized logo badge and header.
  - **Low Latency**: Dedicated toggle pill button. When enabled, switches the pitch detector and overlap-add pitch shifter to low-latency processing (~128 samples / ~2.6 ms), reporting accurate latency to the DAW.

- **Main Arc Pitch Display**:
  - **Cent Deviation Arc**: Glowing neon-cyan arc with calibration marks at `-100`, `-50`, `0`, `+50`, and `+100` cents.
  - **Center Note Readout**: Dark pill box displaying detected / target note name and octave (e.g. `D# 3`).
  - **Real-Time Tuning Needle**: Electric cyan vertical indicator with a horizontal cross-target marker that swings along the arc at 60 FPS according to real-time cent deviation.

- **Bottom Cards**:
  - **Card 1: Correction**
    - Large rotary knob with a glowing amber/gold circular value arc (`0 %` to `100 %`, default `80 %`). Controls pitch correction speed and snap aggressiveness from smooth, transparent gliding to hard, snappy modern auto-tune.
  - **Card 2: Vibrato & Formant Shift**
    - **Vibrato**: Metallic rotary knob controlling synthetic pitch LFO modulation (`0 %` to `100 %`).
    - **Formant Shift**: Metallic rotary knob controlling independent vocal tract resonance shifting (`-12 st` to `+12 st`).
  - **Card 3: Scale & Tuning Engine**
    - **Root Note**: Dropdown selector (`C`, `C#`, `D`, `D#`, `E`, `F`, `F#`, `G`, `G#`, `A`, `A#`, `B`).
    - **Scale / Follow**: "Follow" button dynamically synchronizes active notes.
    - **Interactive Piano Keyboard**: 1-octave keyboard where active scale notes illuminate in bright royal blue. Any key can be clicked to toggle notes in or out of the scale in real time.
    - **Tuning**: Reference pitch calibration box (default `440 Hz`, adjustable from 415 to 465 Hz).
    - **Scale Type**: Dropdown selector supporting `Natural Minor`, `Major`, `Chromatic`, `Harmonic Minor`, `Melodic Minor`, `Dorian`, `Phrygian`, `Lydian`, `Mixolydian`, `Pentatonic Major`, `Pentatonic Minor`, `Blues`, and `Custom`.

---

## 🔬 DSP Engine Details

1. **Pitch Detection**:
   - Monophonic McLeod Pitch Method (MPM) with Normalized Square Difference Function (NSDF).
   - Quadratic parabolic interpolation for sub-sample fundamental pitch estimation.
   - Peak picking with 85% thresholding to eliminate octave doubling and pitch jumping.
   - Voicing detection with RMS silence gate and median filtering.

2. **Pitch Shifting**:
   - Time-Domain Pitch-Synchronous Overlap-Add (TD-PSOLA) engine.
   - Dynamically tracks fundamental period $T_0$ to align grain window boundaries, preventing phase cancellation and robotic chorus artifacts.
   - 4-point cubic Hermite interpolation for fractional delay line sampling.

3. **Formant Filtering**:
   - Multi-band vocal tract resonance filters (F1, F2, F3) for natural or creative vocal character.

4. **Automation & State Recall**:
   - Built on `juce::AudioProcessorValueTreeState` (`APVTS`) with complete XML session serialization.
   - Fully automatable in DAWs.

---

## 🛠️ Building from Source

### Prerequisites
- **CMake** 3.22 or higher
- **C++20** compatible compiler (e.g. Visual Studio 2022 on Windows, Clang on macOS, GCC on Linux)
- **JUCE 8** (automatically configured or cloned via CMake FetchContent)

### Build Commands (Windows)

```powershell
# Generate Visual Studio 2022 project
cmake -B build -G "Visual Studio 17 2022" -A x64

# Compile Release binaries for VST3 and Standalone
cmake --build build --config Release
```

### Build Outputs
- **VST3 Plugin**: `build/VocalTune_artefacts/Release/VST3/Vocal Tune.vst3`
- **Standalone App**: `build/VocalTune_artefacts/Release/Standalone/Vocal Tune.exe`

---

## 🧪 Validation & Testing

Validated with `pluginval` at **Strictness Level 5**:
```
Started validating: Vocal Tune.vst3
Strictness level: 5
-----------------------------------------------------------------
Plugin scanning: PASSED
Open plugin (cold & warm): PASSED
Audio processing (44.1kHz, 48kHz, 96kHz @ buffers 64 - 1024): PASSED
Plugin state save & restore: PASSED
Automation: PASSED
Editor Automation & DPI Awareness: PASSED
ALL TESTS PASSED
```

