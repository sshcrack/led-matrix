# Beat Detection and Post-Processing

This document describes the beat detection and post-processing features added to the LED Matrix Controller.

## Overview

The system now includes:
- Real-time beat detection from audio input
- Post-processing effects that can be triggered by beats or manually
- Cross-platform support (Windows and Linux)
- REST API for manual control

## Beat Detection

### How It Works
The beat detection algorithm analyzes incoming audio spectrum data to identify beats using an energy-based approach:

1. **Energy Calculation**: Focuses on lower frequency bands (bass) where beats are most prominent
2. **History Tracking**: Maintains a sliding window of recent energy values
3. **Threshold Detection**: Triggers when current energy exceeds average energy by a configurable multiplier
4. **Temporal Filtering**: Enforces minimum time between beats to prevent false positives

### Configuration
Beat detection parameters can be adjusted in `AudioVisualizer.h`:

```cpp
struct BeatDetectionParams {
    float energy_threshold_multiplier = 1.5f;  // Energy must be 1.5x average to trigger
    int energy_history_size = 43;              // Number of frames for rolling average
    float min_beat_interval = 0.3f;            // Minimum 300ms between beats
    float decay_factor = 0.95f;                // Future use for energy decay
};
```

## Post-Processing Effects

### Flash Effect
- Quick brightness ramp-up (0.1s) followed by exponential decay
- Brightens all pixels based on intensity parameter
- Default duration: 0.4s, intensity: 0.8

### Rotate Effect
- Rotates the entire image around the center point
- Supports partial and multiple rotations via intensity parameter
- Default duration: 1.0s, intensity: 1.0 (360 degrees)

### Integration
Post-processing effects are applied automatically when beats are detected, or can be triggered manually via REST API.

## REST API Endpoints

### Trigger Flash Effect
```
GET /post_processing/flash?duration=0.5&intensity=0.8
```

### Trigger Rotate Effect
```
GET /post_processing/rotate?duration=1.0&intensity=1.5
```

### Clear All Effects
```
GET /post_processing/clear
```

### Get Status
```
GET /post_processing/status
```

## Cross-Platform Support

### Matrix Version (Raspberry Pi)
- Receives UDP packets with audio spectrum data
- Performs beat detection on spectrum data
- Triggers post-processing effects

### Desktop Version (Windows/Linux)
- Records audio directly (Windows) or receives external data (Linux)  
- Includes same beat detection algorithm
- Currently for development/testing (beat effects are logged)

## Usage Examples

### Automatic Beat Response
1. Start the LED matrix controller
2. Connect an audio source (using the AudioVisualizer client)
3. Play music - the display will flash on detected beats

### Manual Control
```bash
# Trigger a flash effect
curl "http://your-matrix-ip:8080/post_processing/flash?intensity=1.0"

# Trigger a rotation effect  
curl "http://your-matrix-ip:8080/post_processing/rotate?duration=2.0&intensity=2.0"

# Clear all effects
curl "http://your-matrix-ip:8080/post_processing/clear"
```

## Technical Details

### Audio Processing Pipeline
1. Audio data arrives via UDP (from external client)
2. AudioVisualizer plugin processes spectrum data
3. Beat detection algorithm analyzes energy patterns
4. On beat detection, plugin sets internal flag
5. Matrix controller polls plugins each frame
6. Post-processing effects are applied before canvas swap

### Performance Considerations
- Beat detection runs once per audio packet (typically 20-60 Hz)
- Post-processing effects use efficient pixel manipulation
- Rotation effect uses temporary canvas to avoid artifacts
- All effects have limited duration to prevent accumulation

### Thread Safety
- Beat detection flags use mutex protection
- Post-processing effects are applied on main rendering thread
- Energy history is protected during updates

## Future Enhancements

Potential improvements for the beat detection and post-processing system:

1. **Configurable Effects**: Allow users to choose between flash/rotate/both
2. **Beat Sensitivity**: Runtime adjustment of detection parameters
3. **Advanced Effects**: Add more post-processing options (blur, color shift, etc.)
4. **WebSocket Notifications**: Real-time beat events to connected clients
5. **Machine Learning**: More sophisticated beat detection using trained models