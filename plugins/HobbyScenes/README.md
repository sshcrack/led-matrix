# Hobby and seasonal scenes

Native scenery uses a shared float RGB canvas and runs on the Pi, in previews,
and on the desktop scene worker. Outdoor scenes follow the environment Runtime
Input; time of day, weather and season can each be overridden in the web UI.

## Seasonal scenery

- **Forest path** (`season_forest_path`): a winding woodland path, grazing deer,
  seasonal foliage and drifting petals/leaves/fireflies. Clear daylight adds
  broken sunlight on the path; rain leaves small sky-reflecting pools with sparse
  ripples. Snow hides wet-ground details.
- **Cottage** (`season_cottage`): a garden, chimney smoke, windows and a gate lamp.
  Wet paving catches broken warm reflections after dusk. Snow collects along
  the roof ridge, eaves and fence rails, with short icicles under the eaves.
  Decorations follow active occasions or the scene's Decorations override.

Both scenes include fixed time/weather/season variants. The cottage also has
Christmas, Halloween, Easter and New Year's Eve looks. Automatic Mode uses their
real-world-following look; fixed variants are off by default in Automatic Mode.

Static scenery is cached until the environment changes enough to repaint it.
Rain ripples and reflected highlights touch only small areas each frame. Shared
precipitation reduces its particle budget with `render_quality_scale()`.

## Previewing changes

Build the emulator preset, then render the base scenes and their affected looks:

```bash
scripts/generate_scene_previews.sh --scenes season_forest_path,season_cottage
emulator_build/install/bin/preview_gen --scenes season_forest_path/rain,season_cottage/snow --quality 0.6 --output /tmp/seasonal-previews
```

Use `preview_gen --dump-manifest --manifest-out /tmp/scenes.json` to list the
current variants. See [scene execution](../../docs/SCENE_EXECUTION.md) for timing,
quality and remote-contract rules.
