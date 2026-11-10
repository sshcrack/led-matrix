# Ambient Scenes

Provides Starfield, Metablob, Clock, Sorting Visualizer, Bouncing Logo, Neon
Tunnel and Digital Rain. Scene properties and curated looks are listed in the
web UI's **Scenes** page; **Scene Lab** lets you try them before saving a preset.

The plugin also supplies one-tap starting points to the web preset builder
through `web/index.ts`. Seasonal forest and cottage scenes belong to
[HobbyScenes](../HobbyScenes/README.md).

Clock is a manual scene because it contains readable text. Automatic Mode uses
only scenes whose descriptors explicitly opt in.

Build and installation are handled by the repository's CMake presets. See the
[plugin development guide](../../docs/PLUGIN_DEVELOPMENT.md).
