# CPU optimization

## Findings and changes

The detector ran forward/inverse FFTs at the full host sample rate, up to 375
analyses per second in Low Latency mode. At 48/96/192 kHz the FFT sizes were
4096/8192/16384, although its supported fundamental range is only 55–1100 Hz.
This was a major avoidable cost in both modes, especially at high sample rates.

The detector now uses cascaded 2:1 half-band FIR decimators before analysis.
Each 31-tap symmetric filter needs eight symmetric pairs and a centre tap per
output. Common host rates produce an 11.025 or 12 kHz analysis signal and a
1024-point FFT. All buffers and coefficients are prepared outside processing.
The analysis window duration and nominal update cadence are preserved; filtering
adds approximately 1 ms of detector group delay. Audible audio, pitch-shifter
windows, quality settings and host-reported audio latency are unchanged.

Formant processing caches settled coefficients and its smoothing constant, stops
coefficient ramps when complete, and bypasses neutral filters only after exact
identity coefficients and drained filter states. Its control clock continues
through bypass so automation remains independent of host block size.

The pitch display updates at 30 Hz with equivalent animation ballistics, skips
hidden/settled redraws, and the latency status only rebuilds its text and tooltips
when the displayed state changes. DSP benchmark numbers exclude the editor.

## Measurement method

The before binary was preserved before editing DSP. Both binaries use the same
MSVC Release benchmark harness and JUCE checkout. Three alternating before/after
runs are pinned to logical processor 0 (affinity mask 1), use stereo 64-sample
blocks and six seconds of a harmonic gliding vocal-like
tone, and exclude the first second from timing. Preparation and input copies are
excluded. Engine settings are correction 80%, C major, vibrato 0 and formant 0.
Component profiles also exercise the shifter at ratio 1.03 and formant at +2 st.

CSV logs are `build/cpu-pinned-before-{1,2,3}.csv` and
`build/cpu-pinned-after-{1,2,3}.csv`; medians are in `build/cpu-summary.json`.
The benchmark reports elapsed processing/audio percentage, p99/max callback time,
and Windows thread cycles per processed sample. OS scheduling and changing CPU
frequency make wall-clock timing noisy; neither these offline timings nor thread
cycles establish a guaranteed callback deadline or a DAW's total CPU percentage.
Commercial plugins were not benchmarked.

## Results (2026-10-27)

Full-engine medians of three runs. Cycles are Windows thread cycles per processed
sample; elapsed percent is processing time divided by audio duration.

| Host rate | Low Latency | Cycles before | Cycles after | Cycle reduction | Elapsed % before → after |
| --- | --- | ---: | ---: | ---: | ---: |
| 48 kHz | OFF | 5408 | 3449 | 36.2% | 10.01 → 6.59 |
| 48 kHz | ON | 8003 | 3784 | 52.7% | 14.78 → 7.20 |
| 96 kHz | OFF | 6003 | 3296 | 45.1% | 22.63 → 11.87 |
| 96 kHz | ON | 9052 | 3903 | 56.9% | 36.09 → 19.89 |
| 192 kHz | OFF | 6172 | 3030 | 50.9% | 47.27 → 23.44 |
| 192 kHz | ON | 9310 | 3132 | 66.4% | 72.24 → 24.41 |

Across these rates/modes, detector cycles fell 76–94%, settled neutral formant
cycles fell 62–72%, active formant cycles fell 21–37%, and silent-engine cycles
fell 32–47%. The fixed timing instrumentation overhead is included in cycle
counts, so very cheap stages such as bypass are dominated by measurement overhead.
Unpinned exploratory runs are also retained in `build/cpu-before-*.csv` and
`build/cpu-after-*.csv`; scheduling produced much wider variation there. Even
the pinned runs include timing outliers, so the medians are comparative results,
not a real-time deadline guarantee.

## Validation

Regression coverage includes pitch accuracy through 1100 Hz at 44.1/48/96/192 kHz,
anti-alias rejection at every decimation stage, phrase recovery, block partition
independence, formant neutral/active transitions, tail drainage and real-time C++
allocation checks. The existing pitch-shifter suite continues to check reported
latency, unity reconstruction, pitch/level/spectral stability and stereo coherence.
