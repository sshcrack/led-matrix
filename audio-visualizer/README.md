# Audio Visualizer

A real-time audio visualizer application built in Rust that captures audio, performs FFT analysis, and sends the frequency band data via UDP packets to a server.

## Features

- **Real-time Audio Capture**: Captures audio from your default input device
- **FFT Analysis**: Performs Fast Fourier Transform to analyze frequency spectrum
- **Adjustable Frequency Bands**: Configurable number of frequency bands (8-256)
- **UDP Transmission**: Sends frequency data as JSON packets to a configurable server
- **Modern GUI**: Built with egui for a clean, responsive interface
- **Settings Persistence**: Saves configuration to disk automatically
- **Real-time Visualization**: Shows live frequency bars in the application

## Configuration Options

- **Hostname/Port**: Target server for UDP packets (default: 127.0.0.1:8080)
- **Number of Bands**: Frequency resolution (8-256 bands)
- **Gain**: Amplification factor for the audio signal
- **Smoothing**: How much to smooth the frequency data over time
- **Frequency Range**: Minimum and maximum frequencies to analyze (20Hz - 22kHz)

## Data Format

The application sends **ultra-compact binary packets** via UDP for maximum efficiency:

### Packet Structure (8 + N bytes total)
```
Header (8 bytes):
  [0-1]: Magic number (0xAD, 0x01)
  [2]:   Version (0x01)
  [3]:   Number of bands (1-255)
  [4-7]: Timestamp (Unix seconds, little-endian u32)

Data (N bytes):
  [8...]: Band amplitudes (each band as u8: 0-255)
```

### Example Packet Breakdown
- **64 bands**: Only 72 bytes total (8 header + 64 data)
- **32 bands**: Only 40 bytes total (8 header + 32 data)
- **128 bands**: Only 136 bytes total (8 header + 128 data)

### Compression Benefits
- **JSON format**: ~500-2000 bytes per packet
- **Binary format**: ~40-200 bytes per packet
- **Compression ratio**: 10-20x smaller packets!

### Data Precision
- Frequency bands: 8-bit precision (0-255) → converted from float (0.0-1.0)
- Timestamp: Second precision (sufficient for most applications)
- Magic number: Easy packet validation

### Parsing Examples

**Python:**
```python
import struct

def parse_packet(data):
    if len(data) < 8 or data[0] != 0xAD or data[1] != 0x01:
        return None
    
    version, num_bands = struct.unpack('BB', data[2:4])
    timestamp = struct.unpack('<I', data[4:8])[0]
    bands = [b / 255.0 for b in data[8:8+num_bands]]
    
    return {'bands': bands, 'timestamp': timestamp}
```

**C++:**
```cpp
struct AudioPacket {
    uint8_t magic[2];
    uint8_t version;
    uint8_t num_bands;
    uint32_t timestamp;
    std::vector<float> bands;
    
    bool parse(const uint8_t* data, size_t len) {
        if (len < 8 || data[0] != 0xAD || data[1] != 0x01) return false;
        
        version = data[2];
        num_bands = data[3];
        timestamp = *reinterpret_cast<const uint32_t*>(data + 4);
        
        bands.clear();
        for (int i = 0; i < num_bands; i++) {
            bands.push_back(data[8 + i] / 255.0f);
        }
        return true;
    }
};
```

**JavaScript:**
```javascript
function parsePacket(buffer) {
    if (buffer.length < 8 || buffer[0] !== 0xAD || buffer[1] !== 0x01) {
        return null;
    }
    
    const view = new DataView(buffer.buffer);
    const version = buffer[2];
    const numBands = buffer[3];
    const timestamp = view.getUint32(4, true); // little-endian
    
    const bands = [];
    for (let i = 0; i < numBands; i++) {
        bands.push(buffer[8 + i] / 255.0);
    }
    
    return { bands, timestamp, version };
}
```

## Building and Running

### Prerequisites

- Rust 1.70 or later
- Audio system (ALSA on Linux, CoreAudio on macOS, WASAPI on Windows)

### Build

```bash
cargo build --release
```

### Run

```bash
cargo run --release
```

## Usage

1. **Start the application**
   ```bash
   cargo run --release
   ```

2. **Configure settings**
   - Set the hostname and port of your target server
   - Adjust the number of frequency bands as needed
   - Fine-tune gain and smoothing parameters
   - Set the frequency range for analysis

3. **Start visualization**
   - Click "Start" to begin audio capture and UDP transmission
   - The application will show real-time frequency bars
   - Audio data will be sent to the configured server

4. **Test with included server**
   ```bash
   python3 test_server.py
   ```

## Example Server

A simple test server (`test_server.py`) is included that receives and displays the audio data:

```bash
python3 test_server.py
```

This server will:
- Listen on 127.0.0.1:8080
- Display received frequency data as ASCII bars
- Show statistics about the audio signal

## Configuration Storage

Settings are automatically saved to:
- Linux: `~/.config/audio-visualizer/config.toml`
- macOS: `~/Library/Application Support/audio-visualizer/config.toml`
- Windows: `%APPDATA%\audio-visualizer\config.toml`

## Dependencies

- `cpal`: Cross-platform audio I/O
- `rustfft`: Fast Fourier Transform implementation
- `eframe/egui`: GUI framework
- `tokio`: Async runtime for UDP transmission
- `serde/serde_json`: JSON serialization
- `toml`: Configuration file format

## Use Cases

This audio visualizer can be used for:

- **LED Matrix Displays**: Drive LED strips or matrices with audio-reactive lighting
- **Stage Lighting**: Control DMX lighting systems based on audio
- **Data Analysis**: Collect audio frequency data for analysis
- **Art Installations**: Create interactive audio-visual experiences
- **Music Production**: Real-time frequency analysis for mixing

## Troubleshooting

### No Audio Input
- Check that your microphone is properly connected
- Verify default audio input device in system settings
- Try running with `--verbose` flag for debug output

### UDP Packets Not Received
- Ensure the target server is running and listening
- Check firewall settings
- Verify hostname and port configuration
- Test with the included `test_server.py`

### Performance Issues
- Reduce the number of frequency bands
- Increase smoothing factor
- Close other audio applications
- Use release build for better performance

## License

MIT License - see LICENSE file for details.
