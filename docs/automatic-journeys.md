# Automatic visual journeys

Automatic Mode now follows a repeating 24–36 minute composition. The configured Director seed determines its duration and initial motif. Five phases smoothly shape intensity, motion, and scene dwell: settle, explore, rise, crest, and release. Successive journeys rotate through organic, depth, geometric, and particle motifs; motif preference fades away at each cycle boundary.

Journeys influence ranking rather than excluding scenes. Existing runtime eligibility, render-budget penalties, recent-scene penalties, Spotify track introductions, and prepared-video handoffs still apply. Active music retains 82% of the intensity/motion target; without music the journey has more influence. Detected silence keeps the display restrained. Album art and music-video dwell times retain their track-aware timing.

Only time spent running Automatic Mode advances the journey. Manual playback, pinned scenes, and Scene Lab do not advance it. Applying a Director seed resets the journey and selection history. A process restart starts at the beginning; journey position is not persisted. Reproducible playback requires the same seed, input timeline, elapsed playback time, and eligible catalog.

The web Diagnostics page shows the current phase, motif, elapsed time, and progress. `/diagnostics` exposes these under `director.journey`, alongside the composed targets in `director.context` and explanations on ranked candidates.

## Musical sections

The desktop audio analyzer detects sustained changes in the relative spectral shape. A short smoothed profile is compared with a slower reference. A difference must persist for approximately 1.5 beats (bounded to 0.65–1.2 seconds); a six-second cooldown limits repeated events. Startup and sustained silence/resume reacquire the reference without emitting a section.

This recognizes changes in instrumentation without requiring a simultaneous percussive onset. Its profile is normalized independently of the adaptive visual bands, so changing playback volume does not itself imply a new section. It is a timbral-change heuristic, not a verse/chorus classifier. Beat, onset, and drop detection retain their existing logic and wire format.

## Verification

- `automatic_director_smoke`: phase-dependent selection/pacing, reset/repeatability, and six simulated hours of continuous, cadence-independent progression.
- `section_tracker_smoke`: sustained changes, transient/percussion rejection, silence/resume, and multiple update cadences.
- `audio_analyzer_benchmark`: full FFT/feature pipeline, a smooth instrumentation crossfade followed by a volume change, steady drums across 72–180 BPM, silence, noise, resume, and tempo changes.
- Built-in shader smoke tests and the five-frame shader review workflow cover idle and music rendering at 128×128.
