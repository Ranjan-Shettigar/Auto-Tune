# Vendored pitch processing

Signalsmith Stretch 1.3.2 and Signalsmith Linear are included for offline,
reproducible builds. Exact upstream commits are recorded in `revisions.json`.

- Stretch: https://github.com/Signalsmith-Audio/signalsmith-stretch
- Linear: https://github.com/Signalsmith-Audio/linear
- Licenses: `signalsmith-stretch-LICENSE.txt` and `signalsmith-linear/LICENSE.txt`

Both retain their MIT licenses. Include these notices when distributing the plugin.

Local changes in `signalsmith-stretch.h` are marked `VocalTune`:

1. Normalise the inter-channel phase relation before amplitude recovery, so linked
   stereo channels do not take different noise-floor fallbacks.
2. Measure per-bin frequency from inter-frame phase before interpolating, and refine
   spectral peak locations using that measurement. This improves tuning accuracy
   with the shorter windows used by VocalTune.
3. Phase-lock the low-frequency lobe to its peak to avoid one-sided prediction bias
   near DC and independent-bin beating on octave-down bass.

The wrapper uses equal input/output sample counts, a persistent 32-sample FIFO,
separate input and output storage, and a fixed random seed. Configuration and
allocation happen before audio processing. No network access occurs in the plugin.
