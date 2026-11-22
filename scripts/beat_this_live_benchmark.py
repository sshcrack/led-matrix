#!/usr/bin/env python3
"""Evaluate one pinned Beat This! Live checkpoint against local WAV labels."""

import argparse
import functools
import inspect
import json
import subprocess
import time
import wave
from pathlib import Path, PurePosixPath

from audio_benchmark import BEAT_TOLERANCE, percentile, read_beats, score_events, scores, sha256

REVISION = "353065f154b50442b1bddfa5c020a40f0e64c564"
CHECKPOINT_SHA256 = "0aad60ea50d9758c5501029399f096ae22034af2c6a7d7c5e8642d8498e052db"
SAMPLE_RATE = 22050
HOP = 441
HALF_WINDOW = 512


def emission_times(event_frames, frame_count):
    """Decoder timestamps are frame coordinates. Audio arrives one half-window later."""
    return [(index * HOP + HALF_WINDOW) / SAMPLE_RATE for index in event_frames if 0 <= index < frame_count]


def candidate_batches(frame_count, context, block):
    for start in range(0, frame_count, block):
        end = min(start + block, frame_count)
        yield max(0, end - context), start, end


def load_candidate(source, checkpoint):
    import sys

    if sha256(checkpoint) != CHECKPOINT_SHA256:
        raise ValueError("checkpoint SHA-256 differs from the official small Live checkpoint")
    revision = subprocess.check_output(["git", "-C", source, "rev-parse", "HEAD"], text=True).strip()
    if revision != REVISION:
        raise ValueError(f"expected upstream source revision {REVISION}, received {revision}")
    if subprocess.run(["git", "-C", source, "diff", "--quiet", "HEAD", "--", "beat_this"]).returncode:
        raise ValueError("upstream model source must match its pinned revision")

    import torch

    sys.path.insert(0, str(source.resolve()))
    from beat_this.model import roformer
    from beat_this.model.beat_tracker import BeatThis
    from beat_this.model.postprocessor import Postprocessor
    from beat_this.preprocessing import LogMelSpect

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    # Use the upstream mask with PyTorch's CPU SDPA kernel. Cache the mask
    # geometry so short boundary batches do not trigger FlexAttention JIT.
    roformer.Attend._get_block_mask = functools.lru_cache(maxsize=32)(roformer.Attend._get_block_mask)

    def cpu_attention(q, k, v, *, block_mask):
        queries = torch.arange(q.shape[-2], device=q.device)
        keys = torch.arange(k.shape[-2], device=k.device)
        mask = block_mask.mask_mod(None, None, queries[:, None], keys[None, :])
        return torch.nn.functional.scaled_dot_product_attention(q, k, v, attn_mask=mask)

    roformer.flex_attention_compiled = cpu_attention
    # Training metadata contains a POSIX path. Only its data representation
    # is needed, including on hosts where PosixPath itself is unavailable.
    with torch.serialization.safe_globals([(PurePosixPath, "pathlib.PosixPath")]):
        weights = torch.load(checkpoint, map_location="cpu", weights_only=True)
    hyper = weights["hyper_parameters"]
    if not (hyper["causal_convolution"] and hyper["causal_transformer"] and
            hyper["label_shift"] == -1 and hyper["fps"] == SAMPLE_RATE // HOP):
        raise ValueError("checkpoint does not match the Live timing contract")
    parameters = {key: value for key, value in hyper.items() if key in inspect.signature(BeatThis).parameters}
    model = BeatThis(**parameters)
    model.load_state_dict({key.removeprefix("model.").replace("_orig_mod.", ""): value
                           for key, value in weights["state_dict"].items()})
    model.eval()
    return torch, model, LogMelSpect(), Postprocessor("shifted_causal_local_max", fps=hyper["fps"])


def read_pcm(wav, torch):
    with wave.open(str(wav), "rb") as stream:
        if (stream.getframerate(), stream.getnchannels(), stream.getsampwidth()) != (SAMPLE_RATE, 1, 2):
            raise ValueError("expected mono PCM16 WAV at 22050 Hz")
        if stream.getnframes() <= HALF_WINDOW:
            raise ValueError("excerpt is shorter than the model frontend")
        samples = bytearray(stream.readframes(stream.getnframes()))
    return torch.frombuffer(samples, dtype=torch.int16).float().div_(32768)


def predict(pcm, candidate, context, block, profile_hops):
    torch, model, frontend, postprocess = candidate
    frame_count = (len(pcm) - HALF_WINDOW) // HOP + 1
    timing_ms = []
    logits = {"beat": [], "downbeat": []}
    with torch.inference_mode():
        # The centered mel at index i uses PCM through i*HOP + HALF_WINDOW.
        # Drop the final frames for which that input was never received.
        mel = frontend(pcm)[:frame_count]
        start_time = time.perf_counter()
        for left, start, end in candidate_batches(frame_count, context, block):
            output = model(mel[left:end].unsqueeze(0))
            for kind in logits:
                logits[kind].append(output[kind][:, start-left:])
        beat, downbeat = postprocess(*(torch.cat(logits[kind], dim=1) for kind in logits), shift=1)
        replay_rtf = (time.perf_counter() - start_time) / (len(pcm) / SAMPLE_RATE)
        # Probe the actual single-frame model update cost on a warmed, full
        # context. This excludes capture, mel preprocessing and presentation.
        for end in range(context, min(frame_count, context + profile_hops)):
            start_time = time.perf_counter_ns()
            model(mel[end-context:end].unsqueeze(0))
            timing_ms.append((time.perf_counter_ns() - start_time) / 1e6)
    fps = SAMPLE_RATE / HOP
    predictions = {kind: emission_times((round(float(time) * fps) for time in values[0]), frame_count)
                   for kind, values in (("beat", beat), ("downbeat", downbeat))}
    return predictions, timing_ms, replay_rtf


def evaluate(args):
    candidate = load_candidate(args.source, args.checkpoint)
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    if manifest["schema"] != 1:
        raise ValueError("unsupported corpus manifest")
    tracks = []
    for track in manifest["tracks"]:
        annotations = track["annotations"]
        beat_file = annotations.get("beats_file")
        if beat_file is None:
            continue
        wav = args.corpus_root / track["wav"]
        digest = sha256(wav)
        if digest != track.get("sha256", digest):
            raise ValueError(f"{track['id']}: WAV SHA-256 differs from the manifest")
        pcm = read_pcm(wav, candidate[0])
        duration = len(pcm) / SAMPLE_RATE
        label_path = args.corpus_root / beat_file
        beats, downbeats = read_beats(label_path, duration)
        predictions, timing, rtf = predict(pcm, candidate, args.context, args.block, args.profile_hops)
        entry = {"id": track["id"], "split": track["split"], "kind": track["kind"],
                 "sha256": digest, "annotation_sha256": sha256(label_path), "annotations": annotations,
                 "duration_s": duration, "batch_replay_rtf": rtf,
                 "model_hop_wall_ms": {"p50": percentile(timing, .50), "p95": percentile(timing, .95),
                                       "over_20_ms": sum(ms > 20.0 for ms in timing), "samples": len(timing)},
                 "beat": score_events(beats, predictions["beat"], BEAT_TOLERANCE),
                 "predictions": predictions,
                 "false_silence_beats": [time for time in predictions["beat"] if any(
                     span["start_s"] <= time < span["end_s"] for span in annotations.get("silence", []))]}
        if downbeats:
            entry["downbeat"] = score_events(downbeats, predictions["downbeat"], BEAT_TOLERANCE)
        tracks.append(entry)
        print(f"{track['id']}  beat F1 {entry['beat']['f1']}  model hop p95 ms {entry['model_hop_wall_ms']['p95']}",
              flush=True)
    if not tracks:
        raise ValueError("manifest contains no beat-labeled tracks")
    groups = []
    for kind, split in sorted({(track["kind"], track["split"]) for track in tracks}):
        group = [track for track in tracks if (track["kind"], track["split"]) == (kind, split)]
        metrics = {}
        for event in ("beat", "downbeat"):
            labeled = [track[event] for track in group if event in track]
            if labeled:
                metrics[event] = scores(*(sum(track[key] for track in labeled) for key in ("tp", "fp", "fn")))
        groups.append({"kind": kind, "split": split, "tracks": len(group), **metrics})
    result = {"schema": 1, "mode": "bounded_causal_file_replay",
              "manifest_sha256": sha256(args.manifest), "upstream_revision": REVISION,
              "checkpoint_sha256": CHECKPOINT_SHA256, "torch": candidate[0].__version__,
              "context_frames": args.context, "replay_block_frames": args.block, "sample_rate": SAMPLE_RATE,
              "attention_kernel": "sdpa_cpu_upstream_mask",
              "hop_samples": HOP, "lookahead_samples": HALF_WINDOW, "beat_tolerance_s": BEAT_TOLERANCE,
              "tracks": tracks, "aggregate": groups}
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analyzer-source", dest="source", type=Path, required=True)
    for name in ("checkpoint", "manifest", "corpus-root", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--context", type=int, default=256)
    parser.add_argument("--block", type=int, default=32)
    parser.add_argument("--profile-hops", type=int, default=32)
    args = parser.parse_args()
    if not 1 <= args.block <= args.context or args.profile_hops < 0:
        parser.error("expected 1 <= block <= context and a nonnegative profile length")
    try:
        evaluate(args)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, ImportError) as exc:
        parser.exit(2, f"Beat This! Live benchmark: {exc}\n")


if __name__ == "__main__":
    main()
