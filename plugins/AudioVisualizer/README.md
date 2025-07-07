# Audio Visualizer Plugin

This plugin adds real-time audio visualization capabilities to your LED Matrix display.

## Features

- Real-time audio spectrum visualization
- UDP server for receiving audio data from external sources
- Customizable visualization settings
- Compatible with the Rust audio client from the audio-visualizer directory

## Usage

The plugin runs a UDP server on port 8888 to receive audio data packets from external clients. The audio data is visualized as a frequency spectrum display on the LED matrix.

### Audio Spectrum Scene

Displays a real-time audio spectrum visualization with the following configurable properties:

- **smoothing_factor**: Controls how much the visualization is smoothed over time (0.0 - 1.0)
- **bar_width**: Width of each frequency bar in pixels (1-10)
- **gap_width**: Gap between frequency bars in pixels (0-5)
- **mirror_display**: Whether to mirror the visualization horizontally
- **rainbow_colors**: Whether to use rainbow colors for the visualization
- **base_color**: Base color to use when rainbow_colors is disabled
- **falling_dots**: Whether to show falling dots at the peak of each frequency band
- **dot_fall_speed**: Speed at which the peak dots fall (0.01 - 1.0)

## Compatible Clients

### Rust Audio Client

The plugin is designed to work with the Rust audio client located in the `audio-visualizer` directory. The client captures audio from your system and sends it to the LED matrix over UDP.

#### Packet Format

The client uses a compact binary packet format:

- Header (9 bytes)
  - Magic number (2 bytes): 0xAD, 0x01
  - Version (1 byte): 0x01
  - Number of bands (1 byte): Number of frequency bands
  - Flags (1 byte): Bit flags for additional info
    - bit 0: 1 = interpolated bands enabled + logarithmic scale
  - Timestamp (4 bytes): Unix timestamp in seconds
- Audio data (variable length)
  - Band amplitudes: Each band represented as a uint8 (0-255)

## Installation

The plugin is automatically built and installed with the main LED Matrix application.
