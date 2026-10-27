# Glitch and artifact investigation

## Root cause

Both latency settings used the same faulty waveform-overlap implementation. The
setting only changed window/hop sizes, so it could not remove the shared defects.

1. **Non-causal reads:** analysis started around write-position minus 192/384
   samples, then read a full 256/512-sample grain at ratios up to 2. Search offsets
   made this worse. Some interpolation taps referenced input that had not arrived,
   producing zeros or stale data from a previous circular-buffer lap.
2. **Late output writes:** the first low-latency grain wrote output beginning at
   position 64 when playback had already consumed positions through 126. Those
   samples missed their output time and could reappear after the 32768-sample wrap.
3. **Invalid alignment reference:** correlation searched the previous output hop,
   although playback had already cleared much of that region. Selection therefore
   used zeros/partial overlap instead of a valid waveform reference.
4. **Incompatible paths:** unity replaced output slots, while shifted processing
   added windowed grains at different offsets. Switching paths discarded or added
   incompatible tails, causing clicks and modulation.
5. **Wrong compensation:** the dry branch delayed input and also queued output by
   the declared latency. Measured delay was 384 vs declared 192 samples (ON), and
   768 vs declared 384 (OFF).
6. **Real-time and tracking defects:** detector peaks and formant coefficients
   allocated on the audio thread; live mode changes reset DSP. Tracking, retune,
   vibrato and unvoiced timeouts depended on host callback size. Fixed detector
   windows could not follow bass below roughly 94 Hz at 96 kHz or 188 Hz at 192 kHz.

The old code also claimed mutually inconsistent WSOLA, PSOLA and phase-locked
algorithms in its comments/documentation. Those descriptions were not evidence of
correct signal processing.

## Changes

- Replaced the circular-grain scheduler with continuous spectral processing and a
  fixed 32-sample FIFO. Unity, voiced and unvoiced audio share one signal path.
- Use 40/80 ms windows and report backend input latency plus output latency plus
  FIFO delay. Mode configuration occurs only during host preparation.
- Added local backend corrections for short-window pitch estimates, low-frequency
  phase locking, and linked stereo phase normalisation; see ThirdParty/README.md.
- Scale detector windows and hop lengths with sample rate; remove DC before NSDF,
  reject incomplete startup windows, and expire stale pitch history after silence.
  A subsequent [CPU optimization](CPU_OPTIMIZATION.md) applies this analysis to an
  anti-aliased lower-rate signal, preserving its window duration and update cadence.
- Move allocations out of DSP processing. Controls run on a fixed sample clock,
  including when a host exceeds its advertised buffer size.
- Smooth formant automation through neutral, and compute full target pitch error
  independently of the display's clipped cents value.
- Show pending mode changes and measured latency in the editor. Parameter listeners
  mark a flag instead of posting callbacks holding an editor pointer.

## Verification

Native CTest targets measure impulse latency, unity reconstruction, pitch error,
steady gain, spectral energy, stereo polarity/gain, discontinuities, host-block
partition independence, detector recovery and C++ callback allocations. Both modes
are exercised. Detailed results are in the build test logs; run the commands in
README.md to regenerate them.

Release results (2026-10-27): all three CTest suites passed. Test tones from 55 to
440 Hz, with shifts through +/-12 semitones, passed the 7-cent pitch tolerance
at 44.1/48/96 kHz in both modes. Neutral reconstruction error stayed below
0.000002. Stereo polarity/gain tests and block-partition equivalence passed.
Callback C++ allocation counts were zero at 44.1/48/96/192 kHz, including startup
and oversized buffers. The VST3 passed pluginval v0.2.7 at strictness level 5 with
GUI tests disabled; recorded-vocal listening and DAW-session testing remain open.

The old 220 Hz transition probe produced adjacent-sample steps of approximately
0.427 (OFF) and 0.517 (ON) at input amplitude 0.5. The corrected implementation
reduces these to approximately 0.016 in the same probe, without silence gaps.

## Practical limits

The shorter mode has about 41 ms algorithmic latency, not the previous advertised
2.6 ms. The longer mode is about 81 ms. DAW/interface buffering adds to this.
Formant processing remains a resonance EQ approximation. Synthetic regressions and
host validation establish specific engineering properties; they do not demonstrate
commercial-plugin listening quality on every vocal. No customer recording or live
DAW project was provided for auditioning or comparison.
