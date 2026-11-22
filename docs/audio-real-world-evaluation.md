# Real-song audio evaluation for the LED matrix

2026-11-22. Use immediate transients and band energy for the visible response.
Use a reliable beat grid for secondary motion, such as movement phase or
gradual brightness changes. Reserve downbeats for subtle phrase accents
and use music style, section evidence and dwell time for scene changes. Weak
rhythmic evidence should yield continuous energy-driven motion. Sustained
silence, device switches and dropped input should clear the rhythmic lock.

## Recordings and methods

The [pilot corpus](audio-benchmark.md) contains real recorded excerpts. It
includes ten approximately 30-second GTZAN excerpts with annotated beat
times and three GiantSteps excerpts with tempo labels. The [GTZAN excerpt
index](https://github.com/boblsturm/GTZAN/blob/fedc781026cf1a20034830f8071c98d93a422016/index.txt)
identifies the songs. Six held-out excerpts have beat labels. Full
owner-playlist tracks and real capture-to-panel latency are unmeasured.

- The production analyzer is from `c0494bd7c99d`. Reruns use the same labels and
  source WAVs resampled by FFmpeg 8.0.1 to 44.1 and 48 kHz. The current Linux
  default monitor requests 44.1 kHz. Windows capture uses the selected
  device's default sample rate. Each run uses the latest rolling window and
  scores events at its sample endpoint. See [the benchmark](audio-benchmark.md)
  and [the capture implementation](../plugins/AudioVisualizer/desktop/record.cpp).
- The authors' [real-time PLP](https://github.com/groupmm/real_time_plp/tree/27a27f82cf748b7335c2d71d45b75785cba6c4fd)
  uses its default spectral-flux front end, 48 kHz, 512-sample hops, a 6 s
  kernel, a 60–180 BPM range, and zero prediction offset. The replay
  supplies consecutive PCM blocks and timestamps emitted beats at their
  availability. The [paper's higher beat scores](https://transactions.ismir.net/articles/189/files/66fc0db587be3.pdf)
  use a learned activation front end.
- [Beat This! Live](https://github.com/CPJKU/beat_this_live/tree/353065f154b50442b1bddfa5c020a40f0e64c564)
  uses its published small Live checkpoint, batch causal masking, the
  supplied one-frame shifted local-max decoder, and 22.05 kHz PCM. The
  comparison scores the time the detector has enough input to emit each
  event, adding 512 sample frames for its centered mel window. Prefix
  checks compare beat and downbeat logits from shortened PCM with the
  same portion of the full excerpt. The batch pass approximates availability
  on the audio timeline; CPU and scheduling delay must be measured with a
  real streaming worker. [Checkpoint and MIT terms](https://github.com/CPJKU/beat_this_live)
- All beat scores pool matches at ±70 ms and include acquisition from
  the start. Essentia is the whole-track reference described in the
  [benchmark guide](audio-benchmark.md).

## Sample-rate and cadence checks

| Heuristic input | Held-out strict tempo, regular | Held-out strict tempo, jitter/skip | Held-out beat F1, regular | Held-out beat F1, jitter/skip |
| --- | ---: | ---: | ---: | ---: |
| Native rates, mixed | 26.8% | 10.2% | 0.311 | 0.374 |
| 44.1 kHz | 30.8% | 24.2% | 0.348 | 0.339 |
| 48 kHz | 24.5% | 24.6% | 0.315 | 0.334 |

The large native-rate tempo drop mostly disappeared at common capture
rates. At 22.05 kHz the 256-sample gate makes some jittered ticks too short
to analyze. Observed differences can also reflect the different time span of
a fixed 4,096-sample window after resampling. Beat performance remained weak
at all three rates, so cadence changes alone are an insufficient remedy.

PLP scored 0.363 held-out beat F1 at 48 kHz and 17.3% strict tempo, with 328
extra beats across the six held-out excerpts. With 512-sample hops, the
isolated Python run took 2.91 ms median and 5.28 ms p95 per warmed hop on
an Intel Core Ultra 5 225H Linux machine. Pulse stability may be useful
for motion control. These results give little reason to use the reference
PLP beat events as the default source of flashes.

## Held-out beat timing on named recordings

| Recording | Current 44.1 kHz | PLP | Beat This! Live | Essentia whole track |
| --- | ---: | ---: | ---: | ---: |
| Beastie Boys · Bodhisattva Vow | 0.129 | 0.324 | 0.960 | 0.990 |
| Heatwave · Boogie Nights | 0.556 | 0.563 | 0.754 | 0.850 |
| Ani DiFranco · Coming Up | 0.340 | 0.214 | 0.857 | 0.827 |
| James Carter · The Stevedore’s Serenade | 0.450 | 0.364 | 0.992 | 0.932 |
| George Jones · White Lightning | 0.216 | 0.447 | 0.900 | 0.925 |
| C. P. E. Bach · Oboe Concerto in E-flat, Wq 165 | 0.261 | 0.264 | 0.474 | 0.377 |
| **Pooled beat F1** | **0.348** | **0.363** | **0.826** | **0.827** |

Beat This! Live matched 292 beats and emitted 57 extras, compared with 95
matches and 93 extras from the current 44.1 kHz heuristic. Its downbeat F1
was 0.664. “Coming Up” had a 53 ms p95 absolute error among matched beats
before application and display delay. The C. P. E. Bach excerpt remained
difficult and illustrates the need for confidence-based fallback.

The Beat This! Live checkpoint SHA-256 is
`0aad60ea50d9758c5501029399f096ae22034af2c6a7d7c5e8642d8498e052db`.
The batch CPU run used CPython 3.11 and PyTorch 2.8.0+cpu. It verified
prefix agreement at 100, 350 and 750 frames per excerpt. These results
favor Beat This! Live as the first learned desktop candidate.

## Production setup

1. Drive light that reacts to the next sound from the existing onset,
   percussion, spectral and energy features. Limit the prominence of
   rapid repeats in bass-heavy or highly syncopated passages.
2. Keep the current live analysis as the fallback while evaluating
   Beat This! Live as a separate desktop worker. Feed it contiguous PCM
   from the existing capture and bound its queue. Carry sample time, confidence
   and freshness to visual consumers. Use sustained agreement before
   enabling beat-locked motion and fade out that motion as the grid loses
   confidence. Keep Pi rendering and transport lightweight.
3. Compare the fallback and candidate on full, locally available hip-hop,
   rock and party tracks, including sparse sections, quiet acoustic music,
   tempo changes and transitions. Score false accents, timing and recovery,
   then compare blind visual replays and measure trigger-to-light p95 on the
   physical panel on Windows WASAPI and Linux loopback. Spotify style
   metadata can guide scene selection independent of beat timing.
4. Measure latency before predicting ahead. A small, bounded lookahead can
   help a stable grid compensate for display delay. Transient reactions and
   scene motion should continue through uncertain passages and changes in
   tempo.

[Issue #214](https://github.com/sshcrack/led-matrix/issues/214) covers causal
model evaluation and [#222](https://github.com/sshcrack/led-matrix/issues/222)
tracks the shared evaluation and live measurements.

## Long-track and bounded CPU follow-up

The production choice is the existing desktop analyzer for live response.
Beat This! Live is the leading learned comparison on the short pilot.
The CPU implementation met its 20 ms model-hop deadline only at the
shortest context tested. Longer tracks exposed timing failures hidden by
the short pilot.
The evaluated model is a local benchmark dependency. Application builds
continue to use the existing capture and audio analysis.

The [long-track manifest](../tests/fixtures/audio_benchmark/long_tracks.json)
pins three local Harmonix-aligned recording windows. They cover Paramore’s
“The Only Exception” and “crushcrushcrush” and the Black Eyed Peas’
“I Gotta Feeling.” Source videos were fetched with yt-dlp 2026.08.19.
Candidate cuts were aligned against Harmonix reference audio onsets in the
first 30 seconds and checked for consistent offset against spectral flux in
later windows. Matching an edition precisely remains a limitation.
Tracks whose available editions showed appreciable drift or cut differences
were excluded. The longer pilot has no verified hip-hop edition, and
Harmonix is a possible training-data overlap for the model. Treat these
results as a separate stress check. Downbeat scores are omitted for the
recordings where bar alignment is ambiguous. [HarmonixSet source and
annotation methods](https://github.com/urinieto/harmonixset)

The [bounded benchmark](../scripts/beat_this_live_benchmark.py) reuses the
±70 ms scorer. It computes logits in bounded chunks with the published
causal attention mask, a CPU scaled dot-product attention kernel and the
official one-frame peak decoder. Batch replay estimates sample-clock event
availability. Its separate per-hop probe recomputes the bounded model
context for a single new input frame and reports warm p50/p95 wall time and
missed 20 ms deadlines. The probe omits capture, model frontend, networking,
and rendering. Measurements used one PyTorch thread on the Linux desktop
described above.

| Long recording | Current regular | Beat This! Live, 256 frames | Beat This! Live, 16 frames | Essentia whole track |
| --- | ---: | ---: | ---: | ---: |
| The Only Exception | 0.071 | 0.031 | 0.157 | 0.300 |
| crushcrushcrush | 0.027 | 0.044 | 0.012 | 0.995 |
| I Gotta Feeling | 0.061 | 0.044 | 0.002 | 0.021 |
| **Pooled beat F1** | **0.057** | **0.039** | **0.057** | **0.391** |

The 256-frame model produced roughly the expected number of beats on the
long tracks, but the median signed distance to the nearest annotated beat
was 101–104 ms late on all three. This diagnostic includes events outside
the matching tolerance. The scores retain the original emission times.
Recording alignment and detector delay can both contribute to the offset.
The production heuristic estimated the annotated tempo within 4% for 99.4%
of “I Gotta Feeling” while its beat F1 was 0.061.

| Context | Short calibration beat F1 | Highest track p95 model-hop time | Sampled updates over 20 ms |
| --- | ---: | ---: | ---: |
| 16 frames | 0.590 | 7.6 ms | 0 / 128 |
| 32 frames | 0.670 | 24.9 ms | 128 / 128 |
| 64 frames | 0.790 | 43.5 ms | 128 / 128 |
| 128 frames | 0.844 | 84.2 ms | 128 / 128 |

The 256-frame replay scored 0.774 on the six short held-out tracks with
per-track p95 hop times of 167–203 ms. The 16-frame model kept p95 under 7.2
ms in the long run, but pooled beat F1 stayed at 0.057. A stateful inference
worker that reuses convolution and attention state, or a faster backend,
would require its own timing and accuracy comparison before promotion.
Windows timing, physical-panel response, and audio-gap recovery remain
unmeasured in this follow-up.

### Reproduce locally

Place the decoded sources and published annotations outside the worktree.
The commands below use FFmpeg 8.0.1 and Python 3.11 with CPU builds of
PyTorch and torchaudio 2.8.0, plus NumPy 1.23.5, einops 0.8.1 and
rotary-embedding-torch 0.8.9. Pin the Beat This! Live and HarmonixSet
checkouts as shown. The runner verifies the checkpoint bytes and source
revision. The preparation command verifies the source JAMS and WAV hashes.

```sh
work=/tmp/led-matrix-full-tracks
mkdir -p "$work/downloads"
git clone https://github.com/urinieto/harmonixset "$work/harmonix"
git -C "$work/harmonix" checkout 64abeb509429e73d74559fb98e621dac866efea1
git clone https://github.com/CPJKU/beat_this_live "$work/beat_this_live"
git -C "$work/beat_this_live" checkout 353065f154b50442b1bddfa5c020a40f0e64c564
manifest=tests/fixtures/audio_benchmark/long_tracks.json
jq -r '.tracks[].source.youtube_id' "$manifest" |
  while IFS= read -r id; do
    yt-dlp --no-playlist -f 251 -o "$work/downloads/%(id)s.%(ext)s" "https://www.youtube.com/watch?v=$id"
  done
curl -fL 'https://cloud.cp.jku.at/index.php/s/xe57reQoRJzrw9N/download?path=%2F&files=beat_this_live.ckpt' \
  -o "$work/beat_this_live.ckpt"
python3 scripts/prepare_harmonix_benchmark.py --manifest "$manifest" \
  --downloads "$work/downloads" --harmonix "$work/harmonix" --output "$work"
python3 scripts/audio_benchmark.py --manifest "$manifest" --corpus-root "$work" \
  --analyzer emulator_build/plugins/AudioVisualizer/audio_analyzer_benchmark --output "$work/baseline"
python3 scripts/beat_this_live_benchmark.py --manifest "$manifest" --corpus-root "$work" \
  --analyzer-source "$work/beat_this_live" --checkpoint "$work/beat_this_live.ckpt" --output "$work/live.json"
```

Use the Beat This! Live checkpoint terms and training-data notice from
upstream. This evaluation consumes the separately installed files and
publishes aggregate results. [Upstream MIT terms](https://github.com/CPJKU/beat_this_live)
