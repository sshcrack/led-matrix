# Shadertoy Plugin for LED Matrix

This plugin integrates Shadertoy shaders into the LED Matrix system, allowing you to display and interact with a wide variety of Shadertoy visual effects on your LED matrix hardware. It supports both desktop and matrix environments, providing a seamless experience for previewing and running shaders.

## Features
- Fetches and displays shaders from [Shadertoy](https://www.shadertoy.com/)
- Supports random shader selection and prefetching
- Scene properties for customizing shader selection (URL, random mode, page range)
- Efficient caching and asynchronous fetching for smooth operation
- Desktop preview and matrix rendering support

## API Key Recommendation
For best results, it is **recommended** to set the `SHADERTOY_API_KEY` environment variable. This enables the plugin to use the official Shadertoy API for fetching shaders, which is more reliable and robust than HTML scraping.

- You can create an app and obtain your API key at: [https://www.shadertoy.com/myapps](https://www.shadertoy.com/myapps)
- Set the environment variable before running the application:
  ```sh
  export SHADERTOY_API_KEY=your_api_key_here
  ```
- If the API key is not set, the plugin will fall back to HTML scraping, which may be less reliable and subject to Shadertoy website changes.

## How It Works
- The plugin manages scenes that can display a specific shader or select random shaders from a configurable page range.
- It uses a scraper component to fetch shader IDs, either via the API (if the key is set) or by scraping the website.
- Prefetching and caching mechanisms ensure smooth transitions and minimal loading times.
- The desktop component allows previewing shaders before sending them to the matrix.

## Directory Structure
- `desktop/` – Desktop preview and plugin integration
- `matrix/` – Matrix plugin, scene, and scraper logic
- `thirdparty/` – Dependencies, including the shadertoy-headless renderer

## Requirements
- C++23 or later
- [cpr](https://github.com/libcpr/cpr) for HTTP requests
- [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing

## License
See LICENSE files in this directory and thirdparty/ for details.

## Built-in 3D music scenes

- `shader:music_resonance_cathedral`: a rotating crystal surrounded by bowed ribs and luminous halos. Bass opens the structure, percussion travels along the ribs, and drops light the crystal's edges.
- `shader:music_tidal_reactor`: a rotating gyroid sculpture with a warm interior and moving surface veins. Mids thicken the membrane, bass expands it, and percussion/section events illuminate the veins.

Both run on the connected desktop GPU and stream pixels to the Pi. They participate in Automatic Mode and have an idle animation when audio is unavailable. They require no Shadertoy API key.

Render a review set with `scripts/preview_shadertoy_shader.sh <shader.frag>`. The preview executable also accepts an optional final frames-directory argument after the temporal limit, for capturing every rendered frame at 60 FPS:

```bash
xvfb-run -a desktop_build/tools/shadertoy_shader_preview \
  plugins/Shadertoy/shaders/music_tidal_reactor.frag /tmp/reactor.png \
  128 128 360 synthetic -1 /tmp/reactor-frames
```
