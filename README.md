# 🌈 LED Matrix Controller

<div align="center">

Transform your space with a **powerful C++ application** that turns RGB LED matrices into stunning digital canvases. Create mesmerizing visual effects, display real-time data, and control everything remotely with our comprehensive plugin ecosystem.

**✨ Perfect for makers, developers, and digital artists ✨**

> **🎯 Recommended:** 128x128 matrix (four 64x64 panels) + Raspberry Pi 4 for optimal results!

[![GitHub stars](https://img.shields.io/github/stars/sshcrack/led-matrix?style=for-the-badge)](https://github.com/sshcrack/led-matrix/stargazers)
[![License](https://img.shields.io/github/license/sshcrack/led-matrix?style=for-the-badge)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi-red?style=for-the-badge&logo=raspberrypi)](https://www.raspberrypi.org/)

</div>

## ✨ Features

### 🎮 **Comprehensive Control System**
- **REST API server** for seamless remote control
- **React Native mobile app** for on-the-go management  
- **11+ specialized plugins** for unlimited visual variety
- **Preset management** for quick scene switching
- **Advanced scheduling** - Automatically switch presets based on time and day
- **Real-time configuration** without restarts

### 🎨 **Rich Plugin Ecosystem**
Our modular plugin architecture delivers an incredible variety of visual experiences across 11+ specialized plugins:

#### 🌟 **AmbientScenes Plugin**
Create mesmerizing atmospheric effects:
- **Starfield** - Journey through a 3D cosmic environment with customizable stars and twinkling
- **Metablob** - Organic fluid animations with flowing, morphing colors
- **Fire** - Realistic flame simulations with dynamic flickering
- **Clock** - Elegant digital timepieces with customizable styles

#### 🎵 **SpotifyScenes Plugin**
Music comes alive on your matrix:
- **Album Art Display** - Show current track artwork with smooth transitions
- **Beat-synchronized Effects** - Visual rhythms that pulse with your music
- **Now Playing Info** - Track, artist, and playback status visualization
- **OAuth Integration** - Seamless Spotify account connection

#### 🎮 **GameScenes Plugin**
Interactive entertainment with AI-powered gameplay:
- **Tetris** - Neural network AI plays automatically, optimizing piece placement
- **Ping Pong** - Classic Pong with intelligent AI opponents
- **Maze Generator** - Hunt-and-Kill algorithm creates mazes, A* pathfinding solves them

#### 🧮 **FractalScenes Plugin**
Mathematical beauty in motion:
- **Julia Set** - Animated fractal visualizations with evolving parameters
- **Complex Mathematical Patterns** - Stunning algorithmic art

#### 🌦️ **WeatherOverview Plugin**
Real-world data with style:
- **Live Weather Display** - Current conditions with animated effects
- **Weather Animations** - Visual rain, snow, sunshine effects

#### 💻 **GithubScenes Plugin**
Visualize your development activity:
- **Commit Activity** - Show your coding contributions
- **Repository Stats** - Visual representation of your GitHub presence

#### 🎭 **Shadertoy Plugin**
GPU-powered visual effects:
- **Shader-based Animations** - Complex mathematical visualizations
- **Real-time Rendering** - Smooth, performance-optimized effects

#### 🎨 **PixelJoint Plugin**
Pixel art and creative displays:
- **Artistic Visualizations** - Pixel-perfect animations and art

#### 🎆 **RGBMatrixAnimations Plugin**
Physics-based particle systems:
- **Rain Effects** - Realistic precipitation with gravity
- **Spark Systems** - Dynamic particle explosions and trails
- **Gravity Simulations** - Physics-accurate particle behavior

#### 🎵 **AudioVisualizer Plugin**
[Real-time audio analysis](https://github.com/sshcrack/led-matrix/tree/master/plugins/AudioVisualizer):
- **Frequency Spectrum** - Live audio visualization (setup required)
- **Beat Detection** - Rhythm-responsive animations
- **Multi-source Audio** - Various input methods supported

#### 🛠️ **ExampleScenes Plugin**
Development foundation:
- **Template Scenes** - Starting point for custom plugin development
- **Reference Implementation** - Best practices demonstration

#### 🌐 **Image & Media**
- **Remote Image Loading** - Display images from URLs with artistic processing
- **Multiple Format Support** - Handle various image types and sizes
- **Dynamic Content** - Real-time image updates and transformations

### 🔧 **Advanced Features**
- **Hardware abstraction** supporting various matrix configurations
- **Cross-compilation support** for efficient Raspberry Pi deployment
- **Emulator support** with SDL2 for development
- **Configurable logging** with spdlog integration
- **Persistent configuration** with JSON-based settings

### ⏰ **Smart Scheduling System**
Take automation to the next level with intelligent preset scheduling:
- **Time-based switching** - Automatically change presets based on time of day
- **Day-of-week scheduling** - Different configurations for weekdays vs weekends
- **Multiple schedules** - Create unlimited schedules for different scenarios
- **Cross-midnight support** - Schedules that span midnight work seamlessly
- **Mobile management** - Create and edit schedules from your phone
- **Real-time activation** - Schedule changes take effect immediately

**Example Use Cases:**
- **Office hours**: Bright, professional displays during work hours (9 AM - 5 PM, weekdays)
- **Evening ambiance**: Warm, relaxing scenes after sunset (6 PM - 11 PM, daily)
- **Weekend fun**: Colorful, dynamic animations on Saturday and Sunday
- **Night mode**: Dim clock display during sleeping hours (11 PM - 7 AM)

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
- **Schedule management** - Create and manage time-based automation
- **Image upload functionality** for custom displays
- **Cross-platform support** (iOS & Android)

Located in the `react-native/` directory with modern TypeScript and native performance.

## 🛠️ **Hardware Support**

> **⚠️ Important:** This project builds upon the excellent [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) library. For detailed hardware setup, wiring diagrams, troubleshooting, and matrix-specific configuration, please refer to the [comprehensive documentation](https://github.com/hzeller/rpi-rgb-led-matrix) in that repository.

### 🎯 **Recommended Hardware**

> **🌟 Recommended Setup:** For the best experience, we recommend a **128x128 LED matrix** (four 64x64 panels arranged in a 2x2 configuration) paired with a **Raspberry Pi 4**. This setup provides excellent resolution and performance for all visual effects!

- **Raspberry Pi 4** (3B+ minimum) for optimal performance
- **RGB LED matrix panels** with HUB75 interface:
  - **Ideal**: Four 64x64 panels for 128x128 total resolution
  - **Alternative**: 32x32, 64x32, or custom sizes supported
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

### 🚀 **Automatic Installation (Recommended)**

The easiest way to install and configure the LED Matrix Controller is with the provided install script. This script will:
- Download the latest release for your platform
- Guide you through hardware configuration (matrix size, chain, parallel, etc.)
- Optionally set up Spotify integration
- Install the binary to `/opt/led-matrix`
- Set up a systemd service for automatic startup

**To get started, simply run:**

```bash
curl -fsSL https://raw.githubusercontent.com/sshcrack/led-matrix/master/install_led_matrix.sh | bash
```

Or, if you have already cloned the repository:

```bash
chmod +x install_led_matrix.sh
./install_led_matrix.sh
```

The script will ask you for your matrix configuration and any optional features. After installation, the service will start automatically.

---

### 🖥️ **Manual Build & Development**

If you want to build from source or develop locally, follow these steps:

> **💡 Pro Tip:** Building on Raspberry Pi can be slow. Consider [cross-compilation](https://github.com/abhiTronix/raspberry-pi-cross-compilers/discussions/123) for faster development cycles.

1. **Install vcpkg** following the [official guide](https://learn.microsoft.com/vcpkg/get_started/get-started)

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
   npm install
   ```

3. **Launch the app:**
   ```bash
   npm run dev:android        # For Android
   npm run dev:ios            # For iOS
   npm run dev:web            # For web
   ```

## 🎯 **Usage Guide**

### 🚀 **Running the Application**

Download the built binary from GitHub releases (`led-matrix-arm64.tar.gz` for RPI 3 64-bit) and extract it at `/opt/led-matrix`

```bash
sudo ./main [options]
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

> **📖 For comprehensive configuration options**, including troubleshooting flickering displays, timing adjustments, and advanced setups, see the [rpi-rgb-led-matrix documentation](https://github.com/hzeller/rpi-rgb-led-matrix?tab=readme-ov-file#types-of-displays).

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
SPDLOG_LEVEL=debug ./main

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

### ⏰ **Schedule Management**

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/schedules` | List all schedules |
| `GET` | `/schedule?id=<schedule_id>` | Get specific schedule details |
| `POST` | `/schedule?id=<schedule_id>` | Create/update schedule |
| `DELETE` | `/schedule?id=<schedule_id>` | Delete schedule |
| `GET` | `/scheduling_status` | Get scheduling status and active preset |
| `GET` | `/set_scheduling_enabled?enabled=<true\|false>` | Enable/disable scheduling |

#### **Schedule JSON Format**
```json
{
  "id": "work-hours",
  "name": "Work Hours Display",
  "preset_id": "office-preset",
  "start_hour": 9,
  "start_minute": 0,
  "end_hour": 17,
  "end_minute": 30,
  "days_of_week": [1, 2, 3, 4, 5],
  "enabled": true
}
```

**Days of Week**: `0` = Sunday, `1` = Monday, ..., `6` = Saturday

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
SPDLOG_LEVEL=debug ./main

# Test basic functionality
./main --led-rows=32 --led-cols=64 -D0
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
- **[Fluent Emoji by Microsoft](https://github.com/microsoft/fluentui-emoji)** - For the crystal ball emoji used as icon for the desktop app
- **Open source community** - For the countless libraries and tools that power this project

---

<div align="center">

**🌟 Star this repo if you found it helpful! 🌟**

Made with ❤️ for the LED matrix community

</div>
