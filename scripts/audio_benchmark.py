#!/usr/bin/env python3
"""Score production analyzer observations against local, sample-aligned WAV annotations."""

import argparse
import bisect
import csv
import hashlib
import json
import math
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

BEAT_TOLERANCE = 0.070
TEMPO_TOLERANCE = 0.04
LOCK_CONFIDENCE = 0.55
LOCK_SECONDS = 1.0
JUMP_RATIO = 1.10
EVENT_TOLERANCE = {"beat": BEAT_TOLERANCE, "onset": 0.070, "kick": 0.070,
                   "snare": 0.070, "hihat": 0.070, "drop": 0.5, "section": 2.0}
ESSENTIA_SETTINGS = {"method": "multifeature", "minTempo": 40, "maxTempo": 208}


def read_beats(path, duration):
    beats, downbeats = [], []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        seconds, *position = line.split()
        seconds = float(seconds)
        beats.append(seconds)
        if position and position[0] == "1":
            downbeats.append(seconds)
    check_times(beats, duration)
    return beats, downbeats


def check_times(times, duration):
    if any(not (0 <= time <= duration) for time in times) or any(
        previous >= current for previous, current in zip(times, times[1:])
    ):
        raise ValueError("annotation times must be in the WAV and strictly increasing")


def match_events(reference, detected, tolerance):
    """Maximize one-to-one matches, then minimize total absolute timing error.

    Each candidate extends the best chain ending at an earlier prediction.
    Updates are delayed until the next reference so it can only be used once.
    """
    chains = [None] * (len(detected) + 1)

    def best_before(index):
        best = (0, 0.0, None)
        while index:
            candidate = chains[index]
            if candidate and candidate[:2] > best[:2]:
                best = candidate
            index -= index & -index
        return best

    for expected in reference:
        start = bisect.bisect_left(detected, expected - tolerance)
        stop = bisect.bisect_right(detected, expected + tolerance)
        updates = []
        for index in range(start, stop):
            count, cost, previous = best_before(index)
            observed = detected[index]
            updates.append((index + 1, (count + 1, cost - abs(observed - expected),
                                        (expected, observed, previous))))
        for index, candidate in updates:
            while index < len(chains):
                if chains[index] is None or candidate[:2] > chains[index][:2]:
                    chains[index] = candidate
                index += index & -index
    _, _, match = best_before(len(detected))
    pairs = []
    while match:
        expected, observed, match = match
        pairs.append((expected, observed))
    return list(reversed(pairs))


def percentile(values, proportion):
    return sorted(values)[math.ceil(len(values) * proportion) - 1] if values else None


def scores(tp, fp, fn):
    return {
        "tp": tp, "fp": fp, "fn": fn,
        "precision": tp / (tp + fp) if tp + fp else None,
        "recall": tp / (tp + fn) if tp + fn else None,
        "f1": 2 * tp / (2 * tp + fp + fn) if tp + fp + fn else None,
    }


def score_events(reference, detected, tolerance):
    pairs = match_events(reference, detected, tolerance)
    matched_reference = {expected for expected, _ in pairs}
    matched_detected = {observed for _, observed in pairs}
    errors = [(observed - expected) * 1000 for expected, observed in pairs]
    return {
        **scores(len(pairs), len(detected) - len(pairs), len(reference) - len(pairs)),
        "tolerance_s": tolerance,
        "median_signed_ms": statistics.median(errors) if errors else None,
        "p95_absolute_ms": percentile([abs(error) for error in errors], 0.95),
        "missed_s": [time for time in reference if time not in matched_reference],
        "extra_s": [time for time in detected if time not in matched_detected],
    }


def tempo_correct(bpm, reference, factors):
    return any(abs(bpm / (reference * factor) - 1) <= TEMPO_TOLERANCE for factor in factors)


def score_tempo(frames, spans, duration, anchors):
    # Treat each observation as available from its sample endpoint until the
    # next observation. The unobserved startup interval counts as unlocked.
    observations = [(0.0, 0.0, 0.0)] + [
        (frame["time"], frame["bpm"], frame["confidence"]) for frame in frames
    ]
    ends = [time for time, _, _ in observations[1:]] + [duration]
    strict = tolerant = total = 0.0
    lock_intervals = []
    for start, stop, reference in spans:
        index = bisect.bisect_right(ends, start)
        for i in range(index, len(observations)):
            time, bpm, confidence = observations[i]
            if time >= stop:
                break
            left, right = max(start, time), min(stop, ends[i])
            seconds = right - left
            total += seconds
            if tempo_correct(bpm, reference, (0.5, 1, 2)):
                tolerant += seconds
            if tempo_correct(bpm, reference, (1,)):
                strict += seconds
                if confidence >= LOCK_CONFIDENCE:
                    if lock_intervals and abs(lock_intervals[-1][1] - left) < 1e-9:
                        lock_intervals[-1][1] = right
                    else:
                        lock_intervals.append([left, right])
    locks = []
    for anchor in anchors:
        start, stop = anchor["start_s"], anchor["end_s"]
        delay = next((max(left, start) - start for left, right in lock_intervals
                      if min(right, stop) - max(left, start) >= LOCK_SECONDS), None)
        locks.append({"kind": anchor["kind"], "start_s": start, "end_s": stop, "lock_delay_s": delay})
    jumps = []
    for previous, current in zip(frames, frames[1:]):
        before, after = previous["bpm"], current["bpm"]
        if min(before, after) > 0 and max(before, after) / min(before, after) >= JUMP_RATIO:
            jumps.append({"time_s": current["time"], "from_bpm": before, "to_bpm": after})
    return {
        "labeled_seconds": total, "strict_seconds": strict, "octave_tolerant_seconds": tolerant,
        "strict_accuracy": strict / total if total else None,
        "octave_tolerant_accuracy": tolerant / total if total else None,
        "locks": locks, "estimated_jumps": jumps,
    }


def read_observations(binary, wav, cadence, output):
    command = [str(binary), "--observe", str(wav)]
    if cadence == "jitter_skip":
        command.append("--jitter-skip")
    with output.open("w", encoding="utf-8", newline="") as stream, subprocess.Popen(
        command, stdout=subprocess.PIPE, encoding="utf-8"
    ) as process:
        header = process.stdout.readline()
        shutil.copyfileobj(process.stdout, stream)
        if process.wait():
            raise subprocess.CalledProcessError(process.returncode, command)
    metadata = json.loads(header)
    with output.open(encoding="utf-8", newline="") as stream:
        if metadata["schema"] != 1 or metadata["cadence"] != cadence or metadata["sample_rate"] <= 0:
            raise ValueError("incompatible observation metadata")
        rate = metadata["sample_rate"]
        rows = []
        previous = None
        for raw in csv.DictReader(stream):
            row = {key: float(value) for key, value in raw.items()}
            if not all(map(math.isfinite, row.values())):
                raise ValueError("non-finite analysis observation")
            for name in ("sample_end", "sequence", "analyzer_ms", *(k for k in row if k.endswith("_count"))):
                row[name] = int(raw[name])
            row["time"] = row["sample_end"] / rate
            origin = rows[0]["sample_end"] if rows else row["sample_end"]
            if abs(row["analyzer_ms"] - (row["sample_end"] - origin) * 1000 / rate) > 1.001:
                raise ValueError("analyzer clock diverged from sample clock")
            if previous and (row["sample_end"] - previous["sample_end"] < metadata["min_hop_samples"]
                             or row["sequence"] != previous["sequence"] + 1):
                raise ValueError("nonmonotonic observation sequence")
            for event in ("beat", "onset", "drop", "section"):
                counter = event + "_count"
                if row[counter] - (previous[counter] if previous else 0) != row[event]:
                    raise ValueError(f"{event} counter does not match observation")
            rows.append(row)
            previous = row
    if not rows or rows[0]["sample_end"] != metadata["window_samples"] or rows[-1]["sample_end"] > metadata["samples"]:
        raise ValueError("invalid observation coverage")
    return metadata, rows


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_regions(regions, duration):
    last_end = 0.0
    for entry in regions:
        start, end = entry["start_s"], entry["end_s"]
        if not last_end <= start < end <= duration:
            raise ValueError("annotation regions must be ordered, disjoint and within the WAV")
        last_end = end


def score_silence(silence, beats):
    return [
        {**span, "false_beat_times_s": [time for time in beats if span["start_s"] <= time < span["end_s"]]}
        for span in silence
    ]


def score_track(track, root, metadata, frames, reference=None):
    duration = metadata["samples"] / metadata["sample_rate"]
    annotations = track["annotations"]
    result = dict(annotations=annotations, events={}, annotation_files={})
    detected_beats = [frame["time"] for frame in frames if frame["beat"]]
    for kind, regions in annotations.get("events", {}).items():
        check_regions(regions, duration)
        result["events"][kind] = []
        for region in regions:
            start, end = region["start_s"], region["end_s"]
            times = region["times_s"]
            check_times(times, duration)
            if any(not start <= time < end for time in times):
                raise ValueError(f"{kind} label outside its scoring region")
            detected = [frame["time"] for frame in frames if start <= frame["time"] < end and frame[kind]]
            result["events"][kind].append({"start_s": start, "end_s": end,
                                           **score_events(times, detected, EVENT_TOLERANCE[kind])})
    beats = []
    if "beats_file" in annotations:
        if "beat" in result["events"]:
            raise ValueError("use either beats_file or inline beat labels")
        file = root / annotations["beats_file"]
        beats, downbeats = read_beats(file, duration)
        if len(beats) < 2:
            raise ValueError("beat file needs at least two annotated beats")
        result["annotation_files"]["beats_sha256"] = sha256(file)
        result["downbeat_labels"] = len(downbeats)
        result["events"]["beat"] = [score_events(beats, detected_beats, BEAT_TOLERANCE)]

    tempo = annotations.get("tempo", [])
    check_regions(tempo, duration)
    spans = [(span["start_s"], span["end_s"], span["bpm"]) for span in tempo]
    if annotations.get("tempo_from_beats"):
        if spans or not beats:
            raise ValueError("tempo_from_beats needs beats_file and no explicit tempo regions")
        spans = [(start, end, 60 / (end - start)) for start, end in zip(beats, beats[1:])]
    if any(not math.isfinite(bpm) or bpm <= 0 for _, _, bpm in spans):
        raise ValueError("tempo must be finite and positive")
    anchors = annotations.get("locks", [])
    check_regions(anchors, duration)
    result["tempo"] = score_tempo(frames, spans, duration, anchors)
    silence = annotations.get("silence", [])
    check_regions(silence, duration)
    result["silence"] = score_silence(silence, detected_beats)
    gaps = [(right["sample_end"] - left["sample_end"]) * 1000 / metadata["sample_rate"]
            for left, right in zip(frames, frames[1:])]
    result["analysis_cadence"] = {"frames": len(frames), "p95_gap_ms": percentile(gaps, .95),
                                  "max_gap_ms": max(gaps, default=None)}
    if reference is not None:
        bpm, ticks = reference
        beat_regions = annotations.get("events", {}).get("beat", [])
        beat_scores = [score_events(beats, ticks, BEAT_TOLERANCE)] if beats else [
            score_events(region["times_s"],
                         [time for time in ticks if region["start_s"] <= time < region["end_s"]], BEAT_TOLERANCE)
            for region in beat_regions
        ]
        result["essentia_reference"] = {
            "bpm": bpm, "ticks_s": ticks, "events": {"beat": beat_scores} if beat_scores else {},
            "silence": score_silence(silence, ticks),
        }
        tempo = score_tempo([{"time": 0, "bpm": bpm, "confidence": 0}], spans, duration, [])
        result["essentia_reference"]["tempo"] = {key: value for key, value in tempo.items()
                                                 if key not in ("locks", "estimated_jumps")}
    return result


def aggregate(tracks):
    groups = {}
    for track in tracks:
        groups.setdefault((track["kind"], track["split"], track["cadence"]), []).append(track)
    summaries = []
    for (kind, split, cadence), group in sorted(groups.items()):
        summaries.append({"kind": kind, "split": split, "cadence": cadence, **summarize(group)})
    return summaries


def summarize(group):
    events = {}
    for track in group:
        for kind, regions in track["events"].items():
            events.setdefault(kind, []).extend(regions)
    seconds = sum(track["tempo"]["labeled_seconds"] for track in group)
    summary = {
        "tracks": len(group),
        "tempo_labeled_seconds": seconds,
        "tempo_strict_accuracy": sum(track["tempo"]["strict_seconds"] for track in group) / seconds if seconds else None,
        "tempo_octave_tolerant_accuracy": sum(track["tempo"]["octave_tolerant_seconds"] for track in group) / seconds if seconds else None,
        "events": {kind: scores(*(sum(region[key] for region in regions) for key in ("tp", "fp", "fn")))
                   for kind, regions in events.items()},
        "false_silence_beats": sum(len(span["false_beat_times_s"]) for track in group for span in track["silence"]),
        "silence_seconds": sum(span["end_s"] - span["start_s"] for track in group for span in track["silence"]),
    }
    if "locks" in group[0]["tempo"]:
        locks = [lock for track in group for lock in track["tempo"]["locks"]]
        delays = [lock["lock_delay_s"] for lock in locks if lock["lock_delay_s"] is not None]
        summary["tempo_locks"] = {"attempts": len(locks), "acquired": len(delays),
                                  "p95_delay_s": percentile(delays, .95)}
        summary["estimated_jumps"] = sum(len(track["tempo"]["estimated_jumps"]) for track in group)
    return summary


def load_essentia():
    from importlib.metadata import version

    packages = {"essentia": "2.1b6.dev1389", "numpy": "1.26.4", "PyYAML": "6.0.4", "six": "1.17.0"}
    try:
        if any(version(package) != pin for package, pin in packages.items()):
            raise ValueError("Essentia environment needs the versions in docs/audio-benchmark.md")
        import essentia.standard
    except ImportError as exc:
        raise ValueError("install the optional Essentia environment from docs/audio-benchmark.md") from exc

    settings = {"packages": packages, "rhythm_extractor": ESSENTIA_SETTINGS, "sample_rate": 44100}

    def analyze(wav):
        signal = essentia.standard.MonoLoader(filename=str(wav), sampleRate=settings["sample_rate"])()
        bpm, ticks, *_ = essentia.standard.RhythmExtractor2013(**ESSENTIA_SETTINGS)(signal)
        return float(bpm), [float(tick) for tick in ticks]

    return settings, analyze


def report(args):
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest["schema"] != 1:
        raise ValueError("unsupported corpus manifest version")
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    reference = load_essentia() if args.essentia else None
    tracks = []
    ids = set()
    for track in manifest["tracks"]:
        track_id = track["id"]
        try:
            if not track_id or any(c not in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-" for c in track_id) or track_id in ids:
                raise ValueError("invalid or duplicate track ID")
            ids.add(track_id)
            if track["split"] not in ("calibration", "held_out") or track["kind"] not in ("real", "synthetic"):
                raise ValueError("invalid track kind or split")
            annotations = track["annotations"]
            uncertainty = annotations["timing_uncertainty_s"]
            if not annotations["provenance"] or (uncertainty is not None and not 0 <= uncertainty < math.inf):
                raise ValueError("invalid annotation provenance or uncertainty")
            wav = args.corpus_root / track["wav"]
            source_hash = sha256(wav)
            if track.get("sha256", source_hash) != source_hash:
                raise ValueError("WAV SHA-256 does not match the manifest")
            for cadence in ("regular", "jitter_skip"):
                observation = output / f"{track_id}.{cadence}.csv"
                metadata, frames = read_observations(args.analyzer.resolve(), wav, cadence, observation)
                tracks.append({
                    "id": track_id, "kind": track["kind"], "split": track["split"], "tags": track.get("tags", []),
                    "sha256": source_hash, "cadence": cadence, "observation": observation.name,
                    "analyzer": metadata, **score_track(
                        track, args.corpus_root, metadata, frames,
                        reference[1](wav) if reference and cadence == "regular" else None,
                    ),
                })
        except (OSError, KeyError, TypeError, ValueError, subprocess.CalledProcessError) as exc:
            raise ValueError(f"{track_id}: {exc}") from exc
    if not tracks:
        raise ValueError("manifest contains no tracks")
    document = {
        "schema": 1, "mode": "offline_sample_clock", "corpus": manifest["id"],
        "manifest_sha256": sha256(args.manifest), "analyzer_sha256": sha256(args.analyzer),
        "settings": {"event_tolerance_s": EVENT_TOLERANCE, "tempo_relative_tolerance": TEMPO_TOLERANCE,
                     "lock_confidence": LOCK_CONFIDENCE, "lock_seconds": LOCK_SECONDS, "jump_ratio": JUMP_RATIO},
        "tracks": tracks, "aggregate": aggregate(tracks),
    }
    if reference:
        groups = {}
        for track in tracks:
            if "essentia_reference" in track:
                groups.setdefault((track["kind"], track["split"]), []).append(track["essentia_reference"])
        document["essentia_reference"] = {
            "mode": "offline_whole_track", "settings": reference[0],
            "aggregate": [{"kind": kind, "split": split, **summarize(group)}
                          for (kind, split), group in sorted(groups.items())],
        }
    (output / "report.json").write_text(json.dumps(document, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(output / "report.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analyzer", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--corpus-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--essentia", action="store_true", help="also score the optional whole-track offline reference")
    args = parser.parse_args()
    try:
        report(args)
    except (OSError, KeyError, TypeError, ValueError) as exc:
        parser.exit(2, f"audio benchmark: {exc}\n")


if __name__ == "__main__":
    sys.exit(main())
