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
- vcpkg package manager
- React Native development environment (for the mobile app)

## Building

### Backend

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
npm run dev:ios    # Run on iOs
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
````
