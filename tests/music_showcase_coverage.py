#!/usr/bin/env python3
"""Every style the directors cast for needs enough music-showcase scenes.

Reads the scene manifest written by `preview_gen --dump-manifest --manifest-out`
and checks that
  * every scene tagged "music-showcase" declares at least one "style:<name>" tag
    (otherwise style matching can never prefer it), and
  * every MusicStyle has a minimum number of showcase scenes, with a higher bar
    for the styles the owner plays most (German rap, indie/pop, rock/punk).
"""
import collections
import json
import sys

STYLES = ["chill", "edm", "party", "rock", "hiphop", "acoustic"]
MINIMUM = {style: 3 for style in STYLES}
MINIMUM.update({"hiphop": 5, "rock": 5, "party": 5})


def main(path: str) -> int:
    with open(path, encoding="utf-8") as fh:
        manifest = json.load(fh)
    counts = collections.Counter()
    errors = []
    for scene in manifest:
        tags = scene.get("descriptor", {}).get("tags", [])
        if "music-showcase" not in tags:
            continue
        styles = [t.split(":", 1)[1] for t in tags if t.startswith("style:")]
        if not styles:
            errors.append(f"{scene['name']}: music-showcase without a style:<name> tag")
        for style in styles:
            if style not in STYLES:
                errors.append(f"{scene['name']}: unknown style tag style:{style}")
            counts[style] += 1
    for style, minimum in MINIMUM.items():
        if counts[style] < minimum:
            errors.append(f"style:{style} has {counts[style]} showcase scenes, want >= {minimum}")
    for error in errors:
        print("FAIL:", error, file=sys.stderr)
    print("showcase scenes per style:", dict(sorted(counts.items())))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
