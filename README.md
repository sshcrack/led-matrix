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

The main application that controls the LED matrix and provides the REST API.

### React Native App

A mobile application for controlling the LED matrix remotely. Located in the `react-native` directory.

## Prerequisites

- CMake 3.5 or higher
- C++23 compatible compiler
- `jinja2` Python package (can be installed by running `apt install python3-jinja2 -y`)
- vcpkg package manager
- React Native development environment (for the mobile app)

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

## Dependencies

The project requires the following libraries:

- GraphicsMagick
- cURL
- spdlog
- CPR
- libxml2
- RESTinio
- nlohmann_json
- fmt
- uuid_v4

### Installing Dependencies

Vcpkg needs to be installed.

```shell
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX="/usr/local" ..
sudo cmake --install .
```

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