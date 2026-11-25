#!/usr/bin/env python3
"""Prepare pinned local WAVs and beat labels for the long-track benchmark."""

import argparse
import json
import subprocess
from pathlib import Path

from audio_benchmark import sha256


def prepare(track, downloads, harmonix, output):
    source = track["source"]
    video = downloads / (source["youtube_id"] + ".webm")
    label_path = harmonix / "dataset/jams" / source["jams"]
    if sha256(label_path) != source["jams_sha256"]:
        raise ValueError(f"{track['id']}: JAMS label source differs from the manifest")
    record = json.loads(label_path.read_text(encoding="utf-8"))
    duration = source["duration_s"]
    wav = output / track["wav"]
    wav.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        "ffmpeg", "-hide_banner", "-loglevel", "warning", "-n", "-i", str(video),
        "-ss", str(source["offset_s"]), "-t", str(duration), "-ar", "22050", "-ac", "1",
        "-c:a", "pcm_s16le", "-fflags", "+bitexact", str(wav),
    ], check=True)
    if sha256(wav) != track["sha256"]:
        raise ValueError(f"{track['id']}: decoded WAV differs from the evaluated source")
    events = next(entry["data"] for entry in record["annotations"] if entry["namespace"] == "beat")
    rows = []
    for event in events:
        if 0 <= event["time"] < duration:
            position = f"\t{int(event['value'])}" if source["downbeats"] else ""
            rows.append(f"{event['time']:.3f}{position}\n")
    (output / track["annotations"]["beats_file"]).write_text("".join(rows), encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("manifest", "downloads", "harmonix", "output"):
        parser.add_argument(f"--{option}", type=Path, required=True)
    args = parser.parse_args()
    try:
        tracks = json.loads(args.manifest.read_text(encoding="utf-8"))["tracks"]
        for track in tracks:
            prepare(track, args.downloads, args.harmonix, args.output)
    except (ValueError, OSError, KeyError, subprocess.SubprocessError) as exc:
        parser.exit(2, f"prepare Harmonix benchmark: {exc}\n")


if __name__ == "__main__":
    main()
