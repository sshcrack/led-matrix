## Project Overview
This project consists of three main components:
1. **Matrix Control**: There is one controller program for a 128x128 LED matrix. The main entry point is `src_matrix/main.cpp`.
2. **Desktop Application**: The desktop application is based on ImGui and the source code is located in "src_desktop/". The main entry point is `src_desktop/main.cpp`.
3. **React-Native**: This react-native application is used to control the matrix. It'll be used as web version and as mobile app. The source code is in `react_native/`.

### Plugins
Both the desktop application and the matrix load plugins. The plugins are located in the `plugins/` directory. Each plugin has its own subdirectory, e.g., `plugins/AudioVisualizer/`. Matrix plugins are loaded from `plugins/matrix/`, while desktop plugins are loaded from `plugins/desktop/`.

### Shared Libraries
The project uses shared libraries for common functionality. There are three shared libraries:
- `common`: Used for common code between the matrix and the desktop application.
- `matrix`: Contains code specific to the matrix controller.
- `desktop`: Contains code specific to the desktop application.

### Build System
This project uses CMake as the build system and CMake presets are defined in the `CMakePresets.json` file. The presets are used to build the matrix, desktop application, and plugins.
The presets you'll use most often are:
- `emulator`: Builds a emulator of the matrix for testing purposes (this will be compiled with x64 instead of arm64)
- `cross-compile`: Builds the matrix controller application (arm64 cross-compiled for Raspberry Pi)
- `desktop`: Builds the desktop application.
Notice: Use SKIP_WEB_BUILD=ON to skip the web build of the react-native app. This is useful if you haven't made changes to the react-native app and want to speed up the build process.


## Matrix Controller
This is the main controller for the LED matrix. It handles the rendering of images, animations, and other visual effects on the matrix. The controller is designed to be run on a Raspberry Pi or similar device.
The controller hosts a web server that allows to control the matrix via a web interface or a mobile app. The web server is implemented using `restinio` and is located in `src_matrix/server/`.
A UDP socket is also listening, this is used to receive data from the desktop application.

## Desktop Application
The desktop application is built using ImGui and is used to do heavy lifting, such as rendering GPU intensive animations (like GLSL shaders) and host specific visualizations (like the real-time audio visualizer). It communicates with the matrix controller via a UDP socket (for streaming pixel data and custom UDP packets, which share a common class `UdpPacket`). For non-streaming data, it uses a WebSocket connection to the matrix controller.


## React-Native Application
The React-Native application is used to control the matrix from a mobile device. It communicates with the matrix controller via a REST-Interface (you'll need to update the README.md file if you add new endpoints or modify them). This application uses tailwindcss for styling and is designed to be responsive and good looking on both web and mobile devices.