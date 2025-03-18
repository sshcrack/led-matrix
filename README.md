# LED Matrix Controller

A C++ application to control LED matrix displays with various scene effects, image display capabilities, and a REST API interface.

## Features

- REST API server for remote control
- Plugin system for extensible scenes and effects
- Various built-in scenes:
  - Image display
  - Rain effect
  - Watermelon plasma
  - Wave effect
  - Weather display
  - Spotify integration
  - Sparks effect
  - Ping pong game
  - Tetris
  - Maze generator
  - Starfield
  - Metablob effect
  - Fire effect
- Preset management system
- Remote image loading and processing
- Hardware abstraction for LED matrix control

## Components

### C++ Backend

The main application that controls the LED matrix and provides the REST API. It handles:
- Scene rendering and animation
- Plugin loading and management
- Hardware interfacing with LED matrices
- REST API endpoints for remote control
- Configuration persistence

### React Native App

A mobile application for controlling the LED matrix remotely. Located in the `react-native` directory.
- Scene selection and configuration
- Real-time control of matrix displays
- Preset management
- Image uploading

## Hardware Recommendations

This project is designed to work with:
- Raspberry Pi (3B+ or 4 recommended)
- RGB LED matrix panels with HUB75 interface
- Appropriate power supply for your matrix (5V with sufficient amperage)
- [Adafruit RGB Matrix Bonnet](https://www.adafruit.com/product/3211) or similar HAT/Bonnet (recommended for stable performance)

### Matrix Size Configuration

The application supports various matrix sizes and chaining configurations:
- Single panels (typically 32x32, 64x32, or 64x64)
- Multiple panels chained together horizontally and/or vertically
- Configure your matrix dimensions in the command line options or configuration file

## Prerequisites

- CMake 3.5 or higher
- C++23 compatible compiler
- `jinja2` Python package (can be installed by running `apt install python3-jinja2 -y`)
- vcpkg package manager
- React Native development environment (for the mobile app)
- GraphicsMagick and development headers (`apt install libgraphicsmagick1-dev`)

## Building

### Backend
> Note: Building this project on the RPI can be slow, cross-compilation is advised: [Guide](https://github.com/abhiTronix/raspberry-pi-cross-compilers/discussions/123) 

1. Install vcpkg following instructions at https://github.com/Microsoft/vcpkg
2. Configure CMake:
```shell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
```

3. Build:
```shell
cmake --build build
```

4. Install (optional):
```shell
cmake --install build
```

### Emulator Support

To build with emulator support (using SDL2), use the provided CMake preset:

```bash
# Configure and build with emulator support using the preset
cmake --preset=emulator
cmake --build emulator_build
```

This will automatically enable the "emulator" feature in vcpkg and include SDL2 as a dependency.

### Mobile App

1. Navigate to the React Native app directory:
```shell
cd react-native
```

2. Install dependencies:
```shell
npm install
```

3. Run the app:
```shell
npm run dev:android        # Run Android
npm run dev:ios           # Run on iOS
```

## Usage

### Running the Application

Run the application with:

```shell
sudo ./led-matrix-controller [options]
```

Common options:
- `--led-rows=32`: Number of rows per panel
- `--led-cols=64`: Number of columns per panel
- `--led-chain=1`: Number of daisy-chained panels
- `--led-parallel=1`: Number of parallel chains
- `--led-brightness=100`: Set brightness (0-100)
- `--led-gpio-mapping=adafruit-hat`: GPIO mapping for your hardware

### Configuration File

The application uses a `config.json` file in its root directory for persistent settings:

- Scene presets
- Default configurations
- API settings
- Plugin configurations

This file is automatically created if it doesn't exist.

### Logging Configuration

The application uses spdlog for logging. You can control the log level using the `SPDLOG_LEVEL` environment variable:

```shell
SPDLOG_LEVEL=debug ./led-matrix-controller
```

Available log levels:
- `trace`: Most detailed logging
- `debug`: Debug information
- `info`: General information
- `warn`: Warnings
- `error`: Errors
- `critical`: Critical errors
- `off`: Disable logging

All logs are output to the console.

### API Documentation

The REST API is available at `http://<device-ip>:8080/`.

Main endpoints:
- `GET /get_curr`: Get current scene information
- `GET /list_scenes`: List all available scenes
- `GET /list_providers`: List all available image providers
- `GET /set_preset?id=<preset_id>`: Switch to a specific preset
- `GET /presets`: List saved presets (with ?id=<preset_id> to get specific preset)
- `GET /list_presets`: Get detailed information about all presets
- `POST /preset?id=<preset_id>`: Create or update a preset
- `POST /add_preset?id=<preset_id>`: Add a new preset
- `DELETE /preset?id=<preset_id>`: Delete a preset
- `GET /list`: List available images
- `GET /image?url=<url>`: Fetch and process remote image
- `GET /toggle`: Toggle the display on/off
- `GET /skip`: Skip the current scene
- `GET /set_enabled?enabled=<true|false>`: Enable or disable the display
- `GET /status`: Get current matrix status

The application also serves static files from the `/web/` directory.

## Troubleshooting

### Common Issues

1. **Matrix flickering**: Check your power supply rating. LED matrices require substantial current.
2. **Permission errors**: The application needs to be run with sudo to access GPIO pins.
3. **Slow performance**: Consider overclocking your Raspberry Pi if using complex scenes.
4. **Can't connect to API**: Check firewall settings and ensure the correct port is open.

## Plugin Development

### Creating a New Plugin

1. Create a new directory in `plugins/` for your plugin
2. Create the following basic structure:
```
plugins/
└── YourPlugin/
    ├── CMakeLists.txt
    ├── YourPlugin.cpp
    ├── YourPlugin.h
    └── scenes/
        ├── YourScene.cpp
        └── YourScene.h
```

### Plugin Class Structure

Your plugin must implement:
- A main plugin class inheriting from `BasicPlugin`
- One or more scene classes inheriting from `Scenes::Scene`
- Scene wrapper classes for scene instantiation

Example plugin header:
```cpp
class MyPlugin : public BasicPlugin {
public:
    MyPlugin();
    
    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> create_scenes() override;
    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> create_image_providers() override;
};
```

### Scene Properties

Scenes can define configurable properties using the property system:
```cpp
// In your scene class
Property<float> mySpeed{"speed", 1.0f};  // Default value 1.0
Property<int> myColor{"color", 0xFF0000};  // Default color red

// Register in register_properties()
void register_properties() override {
    add_property(mySpeed);
    add_property(myColor);
}
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.