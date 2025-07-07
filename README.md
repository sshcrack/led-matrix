# 🌈 LED Matrix Controller

<div align="center">

A powerful C++ application that transforms RGB LED matrices into dynamic displays with stunning visual effects, real-time data visualization, and remote control capabilities.

[![GitHub stars](https://img.shields.io/github/stars/sshcrack/led-matrix?style=for-the-badge)](https://github.com/sshcrack/led-matrix/stargazers)
[![License](https://img.shields.io/github/license/sshcrack/led-matrix?style=for-the-badge)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi-red?style=for-the-badge&logo=raspberrypi)](https://www.raspberrypi.org/)

</div>

## ✨ Features

### 🎮 **Comprehensive Control System**
- **REST API server** for seamless remote control
- **React Native mobile app** for on-the-go management
- **Plugin architecture** for unlimited extensibility
- **Preset management** for quick scene switching
- **Real-time configuration** without restarts

### 🎨 **Rich Scene Collection**
Our extensive plugin system provides a diverse range of visual experiences:

#### 🌟 **Ambient Scenes**
- **Starfield** - Journey through a 3D cosmic environment
- **Metablob** - Organic fluid animations with flowing colors
- **Clock** - Elegant digital and analog timepieces
- **Fire** - Realistic flame simulations

#### 🎵 **Audio & Music**
- **Spotify Integration** - Display album art with beat-synchronized effects
- [**Audio Spectrum**](https://github.com/sshcrack/led-matrix/tree/master/plugins/AudioVisualizer) - Real-time frequency visualization (Setup required)
- **Beat-synced animations** with BPM detection

#### 🌦️ **Real-World Data**
- **Weather Display** - Live weather with animated effects (rain, snow, etc.)
- **GitHub Activity** - Visualize your development activity

#### 🎮 **Interactive Games**
- **Tetris** - AI-powered automated gameplay
- **Ping Pong** - Classic Pong with AI opponents
- **Maze Generator** - Watch mazes being created and solved

#### 🧮 **Mathematical Art**
- **Julia Set** - Animated fractal visualizations
- **Wave Patterns** - Hypnotic mathematical wave functions
- **Plasma Effects** - Smooth color gradients and patterns

#### 🎆 **Dynamic Effects**
- **Particle Systems** - Rain, sparks, and physics simulations
- **Image Display** - Remote image loading with artistic processing
- **Custom Animations** - Extensible through the plugin system

### 🔧 **Advanced Features**
- **Hardware abstraction** supporting various matrix configurations
- **Cross-compilation support** for efficient Raspberry Pi deployment
- **Emulator support** with SDL2 for development
- **Configurable logging** with spdlog integration
- **Persistent configuration** with JSON-based settings

## 🔌 Components

### 🖥️ **C++ Backend**
The heart of the system - a high-performance application that orchestrates everything:
- **Scene rendering engine** with smooth animations at 60+ FPS
- **Plugin management** with hot-loadable modules
- **Hardware interface** supporting multiple matrix configurations
- **RESTful API server** for external control and integration
- **Configuration persistence** and real-time updates

### 📱 **React Native Mobile App**
A sleek mobile companion for remote control:
- **Intuitive scene selection** with live previews
- **Real-time matrix control** from anywhere on your network
- **Preset management** for quick configuration switching
- **Image upload functionality** for custom displays
- **Cross-platform support** (iOS & Android)

Located in the `react-native/` directory with modern TypeScript and native performance.

## 🛠️ **Hardware Support**

> **⚠️ Important:** This project builds upon the excellent [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) library. For detailed hardware setup, wiring diagrams, troubleshooting, and matrix-specific configuration, please refer to the [comprehensive documentation](https://github.com/hzeller/rpi-rgb-led-matrix) in that repository.

### 🎯 **Recommended Hardware**
- **Raspberry Pi 4** (3B+ minimum) for optimal performance
- **RGB LED matrix panels** with HUB75 interface (32x32, 64x32, 64x64, or custom sizes)
- **Quality power supply** (5V with sufficient amperage - matrices are power-hungry!)
- **[Adafruit RGB Matrix Bonnet](https://www.adafruit.com/product/3211)** or [Electrodragon RGB Panel Driver](https://www.electrodragon.com/product/rgb-matrix-panel-drive-board-for-raspberry-pi-v2/) for reliable performance

### ⚡ **Matrix Configurations**
The application intelligently supports various setups:
- **Single panels**: 32x32, 64x32, 64x64, or custom dimensions
- **Chained displays**: Multiple panels connected horizontally
- **Parallel chains**: Up to 3 chains on Pi 3/4, 6 on Compute Module
- **Mixed configurations**: Different panel sizes and arrangements

Configure your setup using command-line flags or the configuration file - the system adapts automatically!

## 📋 **Prerequisites**

### 🔧 **System Requirements**
- **CMake 3.5+** for build system management
- **C++23 compatible compiler** (GCC 12+ or Clang 15+)
- **vcpkg package manager** for dependency management
- **Python 3** with `jinja2` package (`apt install python3-jinja2 -y`)
- **GraphicsMagick** and development headers (`apt install libgraphicsmagick1-dev`)

### 📱 **For Mobile App Development**
- **Node.js 18+** and npm
- **React Native CLI** and development environment
- **Android Studio** (for Android development)
- **Xcode** (for iOS development on macOS)

## 🚀 **Quick Start Guide**

### 🏗️ **Backend Setup**

> **💡 Pro Tip:** Building on Raspberry Pi can be slow. Consider [cross-compilation](https://github.com/abhiTronix/raspberry-pi-cross-compilers/discussions/123) for faster development cycles.

1. **Install vcpkg** following the [official guide](https://github.com/Microsoft/vcpkg)

2. **Configure the build system:**
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake
   ```

3. **Build the application:**
   ```bash
   cmake --build build
   ```

4. **Install (optional):**
   ```bash
   cmake --install build
   ```

### 🖥️ **Development with Emulator**

Test your scenes without physical hardware using our SDL2-based emulator:

```bash
# One-command setup and build
cmake --preset=emulator
cmake --build emulator_build

# Run with emulation
./emulator_build/main [options]
```

Perfect for development, testing, and demonstrations!

### 📱 **Mobile App Setup**

Get the mobile app running in minutes:

1. **Navigate to the app directory:**
   ```bash
   cd react-native
   ```

2. **Install dependencies:**
   ```bash
   [npm install
   ```

3. **Launch the app:**
   ```bash
   [npm run dev:android        # For Android
   [npm run dev:ios            # For iOS
   npm run dev:web            # For web
   ```

## 🎯 **Usage Guide**

### 🚀 **Running the Application**

Start your LED matrix display with a simple command:

```bash
sudo ./led-matrix-controller [options]
```

> **🔑 Note:** `sudo` is required for GPIO access on Raspberry Pi.

### ⚙️ **Essential Configuration Options**

```bash
# Basic matrix setup
--led-rows=32              # Rows per panel
--led-cols=64              # Columns per panel
--led-chain=2              # Number of chained panels
--led-parallel=1           # Number of parallel chains

# Visual settings
--led-brightness=80        # Brightness (0-100)
--led-pwm-bits=11         # Color depth (1-11)
--led-limit-refresh=120   # Refresh rate limit

# Hardware-specific
--led-gpio-mapping=adafruit-hat    # For Adafruit HAT/Bonnet
--led-slowdown-gpio=1             # Timing adjustment for Pi models
```

> **📖 For comprehensive configuration options**, including troubleshooting flickering displays, timing adjustments, and advanced setups, see the [rpi-rgb-led-matrix documentation](https://github.com/hzeller/rpi-rgb-led-matrix).

### 🗂️ **Configuration Management**

The application uses a smart configuration system:

- **`config.json`** - Automatically created in the application directory
- **Persistent settings** - Scene presets, API configurations, plugin settings
- **Hot-reload support** - Many settings update without restart
- **Backup-friendly** - JSON format for easy version control

### 📊 **Logging System**

Fine-tune logging for development and debugging:

```bash
# Set log level via environment variable
SPDLOG_LEVEL=debug ./led-matrix-controller

# Available levels: trace, debug, info, warn, error, critical, off
```

All logs output to console with timestamps and color coding for easy reading.

## 🌐 **API Reference**

The REST API provides powerful remote control capabilities at `http://<device-ip>:8080/`.

### 📊 **Core Endpoints**

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/status` | System status and current state |
| `GET` | `/get_curr` | Current scene information |
| `GET` | `/list_scenes` | Available scenes and plugins |
| `GET` | `/toggle` | Toggle display on/off |
| `GET` | `/skip` | Skip to next scene |

### 🎛️ **Scene Management**

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/set_preset?id=<preset_id>` | Switch to specific preset |
| `GET` | `/presets` | List all saved presets |
| `POST` | `/preset?id=<preset_id>` | Create/update preset |
| `DELETE` | `/preset?id=<preset_id>` | Delete preset |

### 🖼️ **Media Control**

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/list` | Available local images |
| `GET` | `/image?url=<url>` | Fetch and display remote image |
| `GET` | `/list_providers` | Available image providers |

### ⚙️ **System Control**

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/set_enabled?enabled=<true\|false>` | Enable/disable display |
| `GET` | `/list_presets` | Detailed preset information |

> **🌟 Pro Tip:** The API also serves static files from `/web/` for custom web interfaces!

## 🔧 **Troubleshooting**

### 🚨 **Common Issues & Solutions**

| Problem | Solution |
|---------|----------|
| **Matrix flickering** | Check power supply amperage - LEDs need significant current |
| **Permission errors** | Run with `sudo` for GPIO access |
| **Slow performance** | Try overclocking Pi or reduce `--led-pwm-bits` |
| **Can't connect to API** | Check firewall and ensure port 8080 is open |
| **Panels not lighting up** | Verify `--led-panel-type` setting (try `FM6126A` or `FM6127`) |
| **Colors look wrong** | Adjust `--led-multiplexing` settings (try values 0-17) |

### 🔍 **Advanced Debugging**

```bash
# Show refresh rate for performance monitoring
--led-show-refresh

# Detailed timing information
SPDLOG_LEVEL=debug ./led-matrix-controller

# Test basic functionality
./led-matrix-controller --led-rows=32 --led-cols=64 -D0
```

> **📚 For hardware-specific issues**, timing problems, or panel compatibility, consult the comprehensive [rpi-rgb-led-matrix troubleshooting guide](https://github.com/hzeller/rpi-rgb-led-matrix#troubleshooting).

## 🔌 **Plugin Development**

Extend the matrix with your own custom scenes and effects!

### 🏗️ **Creating Your First Plugin**

1. **Set up the plugin structure:**
   ```
   plugins/
   └── MyAwesomePlugin/
       ├── CMakeLists.txt
       ├── MyAwesomePlugin.cpp
       ├── MyAwesomePlugin.h
       └── scenes/
           ├── MyScene.cpp
           └── MyScene.h
   ```

2. **Implement the plugin interface:**
   ```cpp
   class MyAwesomePlugin : public BasicPlugin {
   public:
       MyAwesomePlugin();
       
       vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> 
           create_scenes() override;
           
       vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> 
           create_image_providers() override;
   };
   ```

### 🎨 **Scene Development**

Create engaging visual experiences with our scene framework:

```cpp
class MyScene : public Scenes::Scene {
private:
    // Configurable properties - automatically exposed in API
    Property<float> speed{"speed", 1.0f};
    Property<int> color{"color", 0xFF0000};
    
public:
    bool render(RGBMatrixBase *matrix) override {
        // Your rendering logic here
        return true; // Continue rendering
    }
    
    void register_properties() override {
        add_property(speed);
        add_property(color);
    }
    
    string get_name() const override { return "my_scene"; }
};
```

### ⚙️ **Advanced Plugin Features**

- **Property system** - Automatic API exposure and persistence
- **Image providers** - Custom image sources and processing
- **Hot-reloading** - Test changes without restarting
- **Inter-plugin communication** - Share data between plugins
- **Custom API endpoints** - Extend the REST API

### 📚 **Plugin Examples**

Study the included plugins for inspiration:
- **`ExampleScenes/`** - Simple starting template
- **`FractalScenes/`** - Mathematical visualizations
- **`SpotifyScenes/`** - External API integration
- **`GameScenes/`** - Interactive content
- **`WeatherOverview/`** - Real-world data visualization

## 🤝 **Contributing**

We welcome contributions! Whether it's a bug fix, new feature, or awesome plugin - join our community.

### 🚀 **Getting Started**

1. **Fork the repository**
2. **Create your feature branch:**
   ```bash
   git checkout -b feature/amazing-new-feature
   ```
3. **Make your changes** with clear, tested code
4. **Commit with descriptive messages:**
   ```bash
   git commit -m 'Add some amazing new feature'
   ```
5. **Push to your branch:**
   ```bash
   git push origin feature/amazing-new-feature
   ```
6. **Open a Pull Request** with detailed description

### 💡 **Contribution Ideas**

- **New scene plugins** - Weather, stocks, social media, games
- **Performance optimizations** - Faster rendering, lower memory usage
- **Hardware support** - New matrix types, different GPIO mappings
- **Mobile app features** - Better UI, offline mode, advanced controls
- **Documentation** - Tutorials, examples, troubleshooting guides

### 📋 **Code Standards**

- **C++23 features** encouraged where appropriate
- **Clear variable names** and comprehensive comments
- **Error handling** with `std::expected` where possible
- **Thread safety** for multi-threaded operations
- **Unit tests** for critical functionality

## 📄 **License**
See the [LICENSE](LICENSE) file for details.

### 🙏 **Acknowledgments**

- **[rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix)** by Henner Zeller - The foundation that makes this all possible
- **[RGBMatrixAnimations](https://github.com/Footleg/RGBMatrixAnimations)** by Footleg - Particle system animations
- **Open source community** - For the countless libraries and tools that power this project

---

<div align="center">

**🌟 Star this repo if you found it helpful! 🌟**

Made with ❤️ for the LED matrix community

</div>
