#!/usr/bin/env python3
"""Scenery scenes must be selectable by Automatic Mode.

`Scene::get_descriptor()` defaults to `automatic_eligible = false`, so a scene
that sets family, tags and `fits:*` but forgets the flag is silently never
chosen and its `fits:*` tags never matter. Reads the scene manifest written by
`preview_gen --dump-manifest --manifest-out` and checks that every scene tagged
`scenery` or `fits:*` is `automatic_eligible`, unless it is listed in OPT_OUT
with the reason it stays manual-only.
"""
import json
import sys

OPT_OUT: dict[str, str] = {}


def main(path: str) -> int:
    with open(path, encoding="utf-8") as fh:
        manifest = json.load(fh)
    scenes = {scene["name"]: scene.get("descriptor", {}) for scene in manifest}
    errors = []
    for name, descriptor in sorted(scenes.items()):
        tags = descriptor.get("tags", [])
        scenic = [t for t in tags if t == "scenery" or t.startswith("fits:")]
        eligible = descriptor.get("automatic_eligible", False)
        if scenic and not eligible and name not in OPT_OUT:
            errors.append(
                f"{name}: tagged {', '.join(scenic)} but automatic_eligible is false "
                "(set d.automatic_eligible = true in get_descriptor(), or add it to OPT_OUT with a reason)"
            )
    for name, reason in OPT_OUT.items():
        if not reason.strip():
            errors.append(f"OPT_OUT entry {name} has no reason")
        descriptor = scenes.get(name)
        if descriptor is None:
            errors.append(f"OPT_OUT entry {name} is not a registered scene")
        elif descriptor.get("automatic_eligible", False):
            errors.append(f"OPT_OUT entry {name} is automatic_eligible now; remove it from OPT_OUT")
    for error in errors:
        print("FAIL:", error, file=sys.stderr)
    checked = sum(
        1 for d in scenes.values()
        if any(t == "scenery" or t.startswith("fits:") for t in d.get("tags", []))
    )
    print(f"{checked} scenery/fits scenes checked, {len(OPT_OUT)} opted out")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
