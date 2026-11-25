#!/usr/bin/env python3
"""Generate original PCM fixtures and exact source-event annotations."""

import argparse
import json
import math
import struct
import wave
from pathlib import Path

RATE = 24000


def region(start, end, **labels):
    return {"start_s": start, "end_s": end, **labels}


def generate(output):
    output.mkdir(parents=True, exist_ok=True)
    audio = [0.0] * (24 * RATE)
    tempo = [region(0.5, 8, bpm=120), region(11, 24, bpm=150)]
    beats = [0.5 + i * 0.5 for i in range(15)] + [11.2 + i * 0.4 for i in range(32)]
    for time in beats:
        start = round(time * RATE)
        # The second half has a gain step and a syncopated triplet figure.
        gain = 0.18 if 16 <= time < 20 else 0.8
        for i in range(RATE // 12):
            t = i / RATE
            audio[start + i] += gain * math.exp(-35 * t) * math.sin(math.tau * 65 * t)
        for offset in (0.0, 0.4 / 3, 0.8 / 3) if 11 <= time < 16 else (0.0,):
            first = start + round(offset * RATE)
            for i in range(RATE // 100):
                audio[first + i] += 0.15 * math.sin(math.tau * 5100 * i / RATE) * (1 - i / (RATE / 100))
    # Sustained bass through the second section. Keeping all samples from 8–11
    # at zero gives the scorer an exact negative window.
    for i in range(11 * RATE, len(audio)):
        audio[i] += 0.08 * math.sin(math.tau * 90 * i / RATE)
    with wave.open(str(output / "pulse-change.wav"), "wb") as wav:
        wav.setparams((1, 2, RATE, len(audio), "NONE", ""))
        wav.writeframes(struct.pack(f"<{len(audio)}h", *(round(sample * 32767) for sample in audio)))

    with wave.open(str(output / "ambient-bed.wav"), "wb") as wav:
        wav.setparams((1, 2, RATE, 8 * RATE, "NONE", ""))
        wav.writeframes(struct.pack(f"<{8 * RATE}h", *(
            round(12000 * math.sin(math.pi * i / (8 * RATE)) ** 2
                  * (0.5 * math.sin(math.tau * 110 * i / RATE) + 0.5 * math.sin(math.tau * 165 * i / RATE)))
            for i in range(8 * RATE)
        )))

    manifest = {
        "schema": 1, "id": "synthetic-audio-benchmark-v1",
        "tracks": [{
            "id": "pulse-change", "wav": "pulse-change.wav", "kind": "synthetic", "split": "calibration",
            "tags": ["tempo_change", "silence_resume", "gain_change", "sustained_bass", "syncopation", "triplets"],
            "annotations": {
                "provenance": "Generated with exact sample positions for kicks and pulse grid.",
                "timing_uncertainty_s": 0,
                "events": {"beat": [region(0, 24, times_s=beats)], "kick": [region(0, 24, times_s=beats)]},
                "tempo": tempo,
                "silence": [region(8, 11)],
                "locks": [region(0.5, 8, kind="acquisition"), region(11, 24, kind="resume_and_tempo_change")],
            },
        }, {
            "id": "ambient-bed", "wav": "ambient-bed.wav", "kind": "synthetic", "split": "calibration",
            "tags": ["ambient", "sparse_intro", "no_pulse"],
            "annotations": {
                "provenance": "Generated slowly changing tonal bed with no beat grid.",
                "timing_uncertainty_s": 0,
                "events": {"beat": [region(0, 8, times_s=[])]},
            },
        }],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    generate(parser.parse_args().output)
