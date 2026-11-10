# Audio Visualizer Plugin

This plugin adds real-time audio visualization capabilities to your LED Matrix display.

## Features

- Real-time audio spectrum visualization
- UDP server for receiving audio data from external sources
- Customizable visualization settings
- System-output capture and analysis through the desktop app

## Usage

The plugin runs a UDP server on port 8888 to receive rich music-analysis packets from the desktop client. That shared analysis feeds both the remaining native audio scenes and the Shadertoy renderer.

The intentionally distinct native visuals are now `audio_spectrum` (bars/radial/waveform/spectrogram) and `audio_particles` (percussion/drop particle bursts). Older fixed tunnel, aurora, and kaleidoscope implementations were removed after their roles were superseded by visually reviewed GPU shaders. Music Director and Automatic Director prefer curated `shader:*` music scenes when the desktop renderer is connected while retaining the native scenes as genuinely different alternatives.

### Audio Spectrum Scene

The `audio_spectrum` scene supports bar, radial, waveform, and spectrogram layouts. The most useful visual-response controls are:

- **smoothing**: Spectrum attack smoothing. Higher values soften fast upward changes.
- **release_speed**: How quickly spectrum bars fall after energy disappears.
- **bar_width / gap_width**: Geometry of the normal bar layouts.
- **mirror_display**: Draw a horizontally mirrored spectrum as a rear layer in normal mode.
- **mirror_layer_brightness**: Brightness of that rear layer. Values around `0.35`–`0.50` keep monochrome/white overlapping bars distinguishable.
- **rainbow_colors / base_color / accent_color**: Palette controls. White for both base and accent is supported; mirror-layer luminance provides the depth cue.
- **falling_dots / dot_fall_speed**: Peak marker behavior.
- **waveform_style**: `TRACE`, `MIRRORED`, or `FILLED`.
- **waveform_gain**: Vertical waveform scale.
- **waveform_stabilization**: Phase-align consecutive capture blocks so the waveform morphs instead of jumping. Enabled by default.
- **waveform_smoothing**: Temporal waveform inertia. `0.82`–`0.92` is a good range for a fluid scope; lower values react faster.
- **waveform_thickness**: Trace thickness in pixels.

Two useful starting points:

- **Fluid waveform:** `display_mode=WAVEFORM`, `waveform_style=MIRRORED`, `waveform_stabilization=true`, `waveform_smoothing=0.88`, `waveform_gain=0.8`, `waveform_thickness=1`.
- **Layered white spectrum:** `display_mode=NORMAL`, `mirror_display=true`, `mirror_layer_brightness=0.42`, `rainbow_colors=false`, `base_color=#ffffff`, `accent_color=#ffffff`, `bar_width=2`, `gap_width=1`, `smoothing=0.68`, `release_speed=3.8`.

## Desktop Audio Plugin

The plugin is designed to work with the desktop cpp audio plugin that is installed with the desktop application of this project. 

The current binary format is defined by
[`shared/common/audio_protocol.h`](../../shared/common/include/shared/common/audio_protocol.h)
and its encoder/decoder implementation. Use that shared contract when writing a
producer; the old band-only packet description no longer applies.

## Installation

Install the [desktop app](../../README.md#desktop-app), set the matrix hostname
and API port (default `8080`), then connect and select the system-output capture
device in the AudioVisualizer tab. Analysis packets use UDP port `8888`; that is
separate from the app's API/WebSocket connection port.
