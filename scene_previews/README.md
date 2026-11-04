# Scene Previews Directory

Animated GIF previews for LED matrix scenes, shown as thumbnails in the web
gallery. **The GIFs are not tracked in git**: every committed regeneration stays
in history forever and slows every clone. They are assets of the rolling
[`scene-previews` release](https://github.com/sshcrack/led-matrix/releases/tag/scene-previews)
and this directory is only a local working copy (`*.gif` is git-ignored).

## Get the previews (fresh clone / before a build)

```bash
./scripts/fetch_scene_previews.sh              # always download
./scripts/fetch_scene_previews.sh --if-missing # only when the directory has no GIFs
```

CMake installs `scene_previews/*.gif` into the package, so fetch before building
a `.deb`. CI does this automatically for the cross-compile job.

## Generate and publish

```bash
# Generate (all, or just the scenes you changed)
./scripts/generate_scene_previews.sh --all
./scripts/generate_scene_previews.sh --scenes "WaveScene,ColorPulseScene"

# Review the GIFs, then publish (needs `gh auth login`)
./scripts/publish_scene_previews.sh
```

Publishing uploads only changed files, removes assets of deleted scenes, and
refreshes `scene_previews.tar.gz`. It never touches git history.
Deploy with: `cmake --build emulator_build --target install`.
