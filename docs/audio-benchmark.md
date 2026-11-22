# Music analyzer benchmark

Build the production analyzer harness with the emulator preset on Linux. The
`audio_analyzer_benchmark` target also builds with the desktop presets on Linux
and Windows. Python 3 is sufficient for the scorer.

```sh
cmake --preset emulator -DSKIP_WEB_BUILD=ON
cmake --build --preset emulator --target audio_analyzer_benchmark
ctest --test-dir emulator_build -R '^audio_(analyzer_benchmark|benchmark_report)$' --output-on-failure
```

Run both analysis schedules and write the per-track observations and report to
a local directory.

```sh
python3 scripts/audio_benchmark.py \
  --analyzer emulator_build/plugins/AudioVisualizer/audio_analyzer_benchmark \
  --manifest tests/fixtures/audio_benchmark/real_tracks.json \
  --corpus-root /path/to/local/corpus \
  --output /tmp/led-matrix-audio-report
```

`report.json` includes the binary SHA-256, build revision and version, settings,
manifest hash, annotation provenance, annotation file hashes and per-track WAV
hashes. It also retains missed and extra beats, signed event timing offsets,
tempo acquisition failures, jumps and the observations used to calculate
them. Reconfigure before building when comparing different commits so the
configured revision matches the source. For Windows use the corresponding
desktop build executable and paths.

## Local corpus

The checked-in manifest identifies a small pilot split fixed by excerpt ID
before scoring. It covers GTZAN hip-hop, rock, disco, country, acoustic
classical with variable tempo, jazz, and GiantSteps electronica, electro-house
and chill-out. GiantSteps track 172384 has competing tempo annotations at 84
and 170 BPM. Its majority label is the strict target and both label weights
are retained in the manifest. The split measures these excerpts and does not
establish owner-playlist or population-wide accuracy.

1. Put the original GTZAN PCM16 WAVs at `gtzan/genres/<genre>/<genre>.<id>.wav`
   below the corpus root. The manifest pins each WAV so differently encoded
   excerpts or shifted cuts are rejected. The
   [GTZAN archive mirror](https://huggingface.co/datasets/m-a-p/GTZAN/blob/3b38eca127a9d82ab91ea1b5740add78f4cd9305/gtzan-dataset-music-genre-classification.zip)
   uses `Data/genres_original/<genre>/` for the equivalent files.
2. Place the [Beat This! annotation checkout](https://github.com/CPJKU/beat_this_annotations/tree/fd9fcc0896cb78730bae735102c8753ef3a3badd)
   at `annotations/`. The GTZAN annotations are credited to [Böck, Davies and Knees](https://github.com/superbock/ISMIR2019).
3. Obtain the GiantSteps previews `1030011.LOFI.mp3`, `5377710.LOFI.mp3` and
   `172384.LOFI.mp3` from the [dataset authors' audio mirror](https://www.cp.jku.at/datasets/giantsteps/backup/).
   Verify the source MD5s in the [GiantSteps dataset](https://github.com/GiantSteps/giantsteps-tempo-dataset/tree/d51ab2422e76abacfaa86616a57054bc222ec9fd),
   then decode the same first 30 seconds using FFmpeg 8.0.1. An
   exact decode is identified by the WAV SHA-256 in the manifest.

   ```sh
   mkdir -p /path/to/local/corpus/edm
   ffmpeg -i 1030011.LOFI.mp3 -t 30 -c:a pcm_s16le -fflags +bitexact /path/to/local/corpus/edm/1030011.wav
   ```

Keep recordings and third-party labels local. Publish metrics or silent visual
evidence. For owner-selected tracks, group calibration and held-out excerpts
by original recording and keep held-out labels fixed across detector changes.
Use the same raw samples for each detector. The `.audr` output of
`audio_analysis_dump` contains derived analysis for `director_music_sim` and
scene replay. Evaluating a detector takes the original audio samples.

The redistributable fixtures are generated from source. They cover a sparse
start, kick timing, sustained bass, syncopated triplets, gain change, silence
and resume at a different tempo, and an ambient bed with no pulse annotation.

```sh
python3 tests/fixtures/audio_benchmark/generate.py /tmp/led-matrix-audio-fixtures
```

Run the same report command with that directory as the corpus root and its
`manifest.json` as the manifest. The CTest uses these fixtures and checks scoring,
early spectral and onset response, skipped-frame timing, and counter and clock
behavior across an analyzer reset with a new sample rate.

## Labels and metrics

- Each manifest row declares `id`, `wav`, `kind` (`real` or `synthetic`),
  `split` (`calibration` or `held_out`) and `annotations`. Label provenance
  describes the annotator or source, revision, and labeling method.
  `timing_uncertainty_s` records a known timing uncertainty or `null` when
  unpublished. Use an optional `sha256` to pin exact WAV bytes.
- All times are seconds from the WAV's first sample. A `beats_file` covers the
  complete excerpt and accepts one beat time per line with an optional second
  column for bar position. Position 1 is a downbeat label. The analyzer emits
  beat events and a phase feature, so this report scores beat timing and
  records downbeat label counts. Section transitions are a separate event.
- Inline `events` map `beat`, `onset`, `kick`, `snare`, `hihat`, `drop`
  or `section` to intervals with `start_s`, `end_s` and `times_s`.
  Annotate every instance in a scored interval. An empty list marks a
  verified negative interval. Omit unsupported event types. `silence`
  separately identifies explicit silent intervals in which to count false
  beats. Interval endpoints are start-inclusive and end-exclusive.
- `tempo` takes intervals with a `bpm`, or `tempo_from_beats` uses the
  reciprocal duration of each annotated beat interval. Strict accuracy is
  the fraction of labeled time within 4% of the annotated tempo. The
  octave-tolerant score also accepts half and double tempo. Each BPM
  observation holds until the next sample endpoint and initial unobserved
  time is unacquired. `locks` declares when to measure acquisition or
  recovery and its deadline, using `start_s`, `end_s` and a descriptive
  `kind`. A lock requires one continuous second of strict tempo accuracy
  with confidence at least 0.55. Estimated jumps are adjacent nonzero BPM
  estimates with a ratio of at least 1.10.
- Events match one-to-one within ±70 ms for beats and transients, ±500 ms for
  drops and ±2 s for section changes. Matching maximizes count and then
  minimizes total absolute error. Report precision, recall, F1, median signed
  and p95 absolute timing errors, and missed and extra event times. Negative
  timing error means an early detection. Aggregate tempo scores weight labeled
  duration and aggregate event scores pool counts within each split and
  cadence. Unscored or undefined metrics are absent or null.
- The harness feeds the latest 4096 samples at nominal 60 Hz with the recorder's
  256-sample minimum hop. Jitter/skip adds repeating offsets of 0, +3, −3, +1,
  −1 ms and drops tick 17 of every 53 (zero-based). At 22.05 kHz, some jittered
  intervals also fall below the minimum hop. The report records observed gaps.
  Each event is scored at its analysis window's sample endpoint. The analyzer's
  relative sample clock and event counters are retained and checked on every
  frame. The export includes no packet-send timestamp or interpolated beats.

### Optional whole-track reference

Use an isolated Python 3.12 environment and add `--essentia` to the report
command. The scorer checks the pins and uses a mono 44.1 kHz decode with
`RhythmExtractor2013`, multifeature, 40–208 BPM. It scores beat ticks using
the same labels and tolerance, and applies the reference's single BPM to the
labeled tempo intervals. The separate reference summary describes offline
whole-track access. Acquisition and recovery comparisons require a causal
reference.

```sh
python3.12 -m venv /tmp/led-matrix-essentia
/tmp/led-matrix-essentia/bin/python -m pip install \
  essentia==2.1b6.dev1389 numpy==1.26.4 PyYAML==6.0.4 six==1.17.0
```

Essentia is available under AGPLv3 and a proprietary license. This benchmark
uses the separately installed package locally and publishes only metrics.
The app and CI artifacts have no Essentia dependency. Any later redistribution
or integration requires its own license evaluation. [Essentia overview](https://essentia.upf.edu/documentation.html) · [RhythmExtractor2013 settings](https://essentia.upf.edu/reference/std_RhythmExtractor2013.html)

## Baseline — 2026-11-22

Linux x86-64, led-matrix 1.22.7 analyzer from `f75ee3e167e1`, on the 13 real
excerpts pinned in the manifest. Tempo uses all excerpts. Beat F1 uses four
calibration and six held-out excerpts with beat labels.

| Method | Split | Strict tempo | Half/double tolerant | Beat F1 |
| --- | --- | ---: | ---: | ---: |
| Heuristic, regular | Calibration | 52.9% | 68.4% | 0.414 |
| Heuristic, jitter/skip | Calibration | 25.2% | 25.2% | 0.368 |
| Heuristic, regular | Held-out | 26.8% | 40.0% | 0.311 |
| Heuristic, jitter/skip | Held-out | 10.2% | 21.0% | 0.374 |
| Essentia, whole track | Calibration | 73.9% | 74.1% | 0.877 |
| Essentia, whole track | Held-out | 82.8% | 95.4% | 0.827 |

1. Prioritize causal beat alignment and missed beats. In regular cadence,
   `gtzan_disco_00015` scores 94.9% strict tempo, 0.413 beat F1, and 68.6 ms
   p95 absolute timing error among matched beats. `gtzan_hiphop_00015` never
   acquires correct tempo, has 0% strict tempo accuracy and scores 0.235 beat F1.
2. Investigate cadence and sample-rate sensitivity before tuning tempo
   thresholds. Held-out strict tempo drops from 26.8% to 10.2% under jitter/skip
   while beat F1 rises. Compare the same excerpts at common capture rates
   and isolate minimum-hop effects before attributing the change to jitter.
3. Retain both tempo scores. In regular cadence, `giantsteps_172384` scores
   0% strict and 94.1% octave tolerant. Essentia returns 166.44 BPM against
   the 84 BPM majority label. This excerpt has no scored beat labels.

The small pilot includes chill-out with an ambiguous pulse; the no-pulse
ambient check is generated. Real owner-selected ambient and additional labeled
instrument and section cases would broaden the evidence. The offline reference
has access to future samples. Causal delivery and the panel response still need
separate evidence for a detector decision.

## Live measurement protocol

The pilot measured sample-clock accuracy on Linux. Live capture latency, CPU,
memory, network behavior, and visible output on Windows and Linux are
unmeasured. Keep any future live report separate from `report.json`. Capture
device, sample rate, backend, build, host, panel mode, transport and clock
source belong in each run's metadata.

1. Play the same generated, sample-indexed transient sequence through the
   normal Windows WASAPI or Linux output-monitor path in a dedicated audio
   preset. Use a local session trace of the existing capture callback and
   completed analysis with monotonic times and sample indices. Record callback
   discontinuities and intervals between successive analyzed sample endpoints
   separately. The desktop's buffered-latency display describes buffer depth.
2. Use local packet arrival timestamps and sequence numbers to compute
   packet loss and p50/p95 packet interarrival intervals and jitter against the
   sender spacing. A one-way transit time requires a synchronized clock.
   Sample process CPU over fixed one-second intervals and report p50/p95
   with the CPU denominator, per-stage latency p50/p95, and peak process RSS
   on Linux or working set on Windows.
3. Measure to the panel with a common-clock audio reference and photodiode on
   an audio-reactive pixel. Record p50/p95 trigger-to-light delays and timeouts.
   A camera reading requires a frame rate and measurement uncertainty. Keep
   the regular setup and loaded-host repetitions separate. Offline elapsed
   time and packet arrival times measure different stages from visible output.
