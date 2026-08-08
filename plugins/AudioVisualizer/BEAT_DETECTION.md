# Music analysis and beat tracking

The AudioVisualizer desktop plugin performs the music analysis for both dedicated audio scenes and audio-reactive scenes in other plugins. The matrix receives the resulting feature frame over the shared audio UDP protocol; it does not re-run tempo analysis.

## Analysis pipeline

Each captured audio window produces:

- adaptive loudness and RMS/peak levels
- seven musical bands from sub-bass through air
- spectral centroid, rolloff, flatness and spectral flux
- onset strength and kick/snare/hi-hat envelopes
- BPM, beat phase, beat confidence and tempo stability
- stereo width, balance and correlation
- drop and section-change events
- a compact waveform and display spectrum

Timing is derived from the capture sample sequence rather than the desktop render clock. This keeps onset intervals and BPM estimation stable when rendering or packet production jitters.

## Tempo tracking

Tempo tracking uses the spectral-flux onset stream, but the raw onset stream is not treated as a sequence of beats:

1. Nearby novelty peaks are clustered in a 220 ms tempo-only window. If a stronger transient arrives after a weak precursor, the stronger transient replaces it.
2. A rolling 12 second window scores candidates from 60 to 180 BPM in 0.5 BPM steps.
3. Each candidate is tested against several following transient intervals and against a stronger-accent subset. This allows quarter-note tempo to survive busy eighth-note activity and missed beats.
4. The winning period is cross-checked against progressively stronger transient subsets. A tempo that remains periodic when weaker novelty events are removed receives more confidence.
5. Recent winning estimates are tracked with a median/MAD stability measure instead of treating one histogram bin as the entire confidence value.
6. 170-180 BPM candidates receive a conservative half-tempo check for the common 85-90 BPM double-time ambiguity. The correction needs repeated independent support and never fires for a strongly-supported genuine fast tempo.

`BeatConfidence` describes how strongly the current periodic evidence supports the BPM. `TempoStability` describes how consistently the tracker has returned the same tempo recently. They are intentionally separate: an ambiguous groove may have a stable candidate while still having low confidence.

When transients disappear, confidence and stability decay instead of leaving a stale locked tempo behind.

## Beat events and phase

Before a tempo is locked, percussive onsets can produce beat events so scenes remain responsive during acquisition. Once confidence and stability are high enough, beat scheduling follows the tracked period and uses tighter minimum spacing so subdivisions do not create extra beat events.

Scenes should not use BPM unconditionally. Tempo-driven motion should blend toward BPM only when both `BeatConfidence` and `TempoStability` indicate a useful lock; otherwise use onset, percussion and energy features directly.

## Diagnostics

The desktop AudioVisualizer panel shows:

- BPM plus `LEARNING`, `TRACKING` or `LOCKED` state
- beat confidence and tempo stability
- beat phase
- kick, snare and hi-hat strengths
- spectral and stereo diagnostics
- history plots for dynamics, tempo and tempo quality

The matrix `/diagnostics` endpoint also exposes BPM, beat confidence, tempo stability, percussion, bands, stereo features and transport health. The web diagnostics page surfaces confidence and stability together.

## Regression benchmark

Desktop builds provide `audio_analyzer_benchmark`, which links the same `MusicAnalyzer.cpp` used by the plugin.

With no arguments it validates synthetic drum material across slow and fast tempos, silence rejection, beat-event cadence, and stale-tempo decay:

```bash
cmake --build --preset desktop-linux --target audio_analyzer_benchmark
./desktop_build/plugins/AudioVisualizer/audio_analyzer_benchmark
```

A PCM 16-bit WAV can be analyzed directly. Supplying an expected BPM turns it into a validation check:

```bash
./desktop_build/plugins/AudioVisualizer/audio_analyzer_benchmark sample.wav 120
```

The optional `AUDIO_BENCH_TRACE=1` environment variable prints detected onset events for analyzer development and debugging.
