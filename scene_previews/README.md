# Scene Previews Directory

This directory contains pre-generated animated GIF previews for LED matrix scenes.
These are used by the web interface to display scene thumbnails in the gallery.

## Generating Previews

To generate or update scene previews:

```bash
# Generate all scenes
./scripts/generate_scene_previews.sh --all

# Generate specific scenes
./scripts/generate_scene_previews.sh --scenes "WaveScene,ColorPulseScene,FractalScene"

# Generate from a list file
./scripts/generate_scene_previews.sh --list my_scenes.txt
```

## Workflow

1. Generate previews using the script above
2. Review the generated GIFs to ensure they look correct
3. Commit the previews to git:
   ```bash
   git add scene_previews/
   git commit -m "Update scene previews"
   ```
4. Deploy with: `cmake --build emulator_build --target install`

## Notes

- Previews are pre-generated and committed to git for consistency across deployments
- Desktop-dependent scenes (e.g., AudioSpectrumScene) cannot be generated here
  - Use `./scripts/capture_desktop_preview.sh` for those scenes
- Preview parameters (FPS, frames, resolution) can be customized via script options
