# 🔌 Plugin Development Guide

Welcome to the LED Matrix Plugin Development Guide! This comprehensive documentation will help you create amazing plugins for the LED Matrix system.

## 📋 Table of Contents

- [🏗️ Plugin Architecture](#️-plugin-architecture)
- [🚀 Quick Start](#-quick-start)
- [📚 Core APIs](#-core-apis)
- [🎨 Scene Development](#-scene-development)
- [🖼️ Image Providers](#️-image-providers)
- [🎭 Post-Processing Effects](#-post-processing-effects)
- [🌐 REST API Integration](#-rest-api-integration)
- [💬 Desktop Communication](#-desktop-communication)
- [⚙️ Properties System](#️-properties-system)
- [🔧 Advanced Features](#-advanced-features)
- [📦 Building and Testing](#-building-and-testing)
- [🎯 Best Practices](#-best-practices)
- [🔍 Examples](#-examples)

## 🏗️ Plugin Architecture

The LED Matrix system uses a modular plugin architecture that supports both **matrix** (Raspberry Pi) and **desktop** applications. These matrix plugin can provide
- **Scenes**: Visual effects and animations
- **Image Providers**: Custom image sources and processing
- **Post-Processing Effects**: Screen-wide effects like flash and rotation
- **REST API Routes**: Custom endpoints for remote control
- **WebSocket Handlers**: Real-time communication with desktop clients

The desktop plugin on the other hand can provide:
**UDP Packets**: these are sent to the matrix, processed and displayed
**Options**: Used for desktop specific options like audio devices
**Websocket Handlers**: Communication with the led matrix server

### Plugin Types

| Type | Description | Platform |
|------|-------------|----------|
| **Matrix Plugins** | Run on the Raspberry Pi controlling the LED matrix | Matrix |
| **Desktop Plugins** | Run on the desktop application for development/control | Desktop |

## 🚀 Quick Start

### 1. Plugin Structure

Create your plugin directory under `plugins/`:

```
plugins/
└── MyAwesomePlugin/
    ├── CMakeLists.txt
    ├── matrix/
    │   ├── MyAwesomePlugin.cpp
    │   ├── MyAwesomePlugin.h
    │   └── scenes/
    │       ├── MyScene.cpp
    │       └── MyScene.h
    └── desktop/        # Optional: Desktop-specific implementation
        ├── MyAwesomePlugin.cpp
        ├── MyAwesomePlugin.h
        └── scenes/
            ├── MyScene.cpp
            └── MyScene.h
```

### 2. Plugin Class

Create your main plugin class inheriting from `BasicPlugin`:

```cpp
// MyAwesomePlugin.h
#pragma once
#include "shared/matrix/plugin/main.h"

class MyAwesomePlugin : public Plugins::BasicPlugin {
public:
    std::string get_plugin_name() const override {
        // This macror is automatically set by CMake
        return PLUGIN_NAME;
    }

protected:
    std::vector<std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>> 
        create_scenes() override;
    
    std::vector<std::unique_ptr<Plugins::ImageProviderWrapper, void (*)(Plugins::ImageProviderWrapper *)>> 
        create_image_providers() override;
};
```

```cpp
// MyAwesomePlugin.cpp

#include "MyAwesomePlugin.h"

// These functions are extremely important! The function names (after `create` and `destroy`) MUST match your plugin name
extern "C" PLUGIN_EXPORT AwesomePlugin *createAwesomePlugin() {
    return new AmbientPlugin();
}

extern "C" PLUGIN_EXPORT void destroyAwesomePlugin(AwesomePlugin *c) {
    delete c;
}
```

### 3. CMakeLists.txt

```cmake
register_plugin(
    MyAwesomePlugin
    matrix/MyAwesomePlugin.cpp
    matrix/scenes/MyScene.cpp
    DESKTOP
# Desktop source files here
)
```

## 📚 Core APIs

### Shared Libraries

The system provides three shared libraries with extensive APIs:

#### `shared/common` - Core Functionality
- **Plugin Loading**: Dynamic library loading and management
- **Utilities**: Common helper functions and data structures
- **Macros**: Plugin creation and registration macros

#### `shared/matrix` - Matrix-Specific APIs
- **Scene Framework**: Base classes for visual effects
- **Canvas API**: Direct pixel manipulation
- **Properties System**: Configurable parameters with automatic serialization
- **Server Integration**: REST API and WebSocket support
- **Post-Processing**: Screen-wide effects and transformations
- **Resource Management**: Font, image, and configuration handling

#### `shared/desktop` - Desktop Application APIs
- **WebSocket Client**: Communication with matrix controller
- **UDP Sender**: High-performance data streaming
- **ImGui Integration**: User interface components
- **Update Management**: Automatic updates and version checking

### Key Include Paths

```cpp
// Core scene framework
#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"

// Properties and configuration
#include "shared/matrix/plugin/property.h"
#include "shared/matrix/plugin/PropertyMacros.h"
#include "shared/matrix/plugin/color.h"

// Utilities
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/utils/utils.h"

// Plugin framework
#include "shared/matrix/plugin/main.h"
#include "shared/common/plugin_macros.h"
```

## 🎨 Scene Development

Scenes are the core visual components that render animations and effects on the matrix.

### Basic Scene Structure

```cpp
// MyScene.h
#pragma once
#include "shared/matrix/Scene.h"
#include "graphics.h"
#include "shared/matrix/utils/FrameTimer.h"

namespace Scenes {
    class MyScene : public Scene {
    private:
        FrameTimer frameTimer;
        
        // Configurable properties
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 1.0f);
        PropertyPointer<rgb_matrix::Color> color = MAKE_PROPERTY("color", rgb_matrix::Color, rgb_matrix::Color(255, 0, 0));
        PropertyPointer<bool> rainbow_mode = MAKE_PROPERTY("rainbow_mode", bool, false);
        
    public:
        MyScene() = default;
        ~MyScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        std::string get_name() const override { return "my_scene"; }
        void register_properties() override;
        
        tmillis_t get_default_duration() override { return 15000; }
        int get_default_weight() override { return 10; }
    };
}
```

### Scene Implementation

```cpp
// MyScene.cpp
#include "MyScene.h"
#include <cmath>

using namespace Scenes;

bool MyScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    auto frameTime = frameTimer.tick();
    float t = frameTime.t * speed->get();
    
    // Clear the canvas
    offscreen_canvas->Clear();
    
    // Get matrix dimensions
    int width = matrix->width();
    int height = matrix->height();
    
    // Render your effect
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t r, g, b;
            
            if (rainbow_mode->get()) {
                // Rainbow effect
                float hue = (x + y + t * 50) / (width + height) * 360.0f;
                hsv_to_rgb(hue, 1.0f, 1.0f, r, g, b);
            } else {
                // Use configured color
                auto col = color->get();
                r = col.r();
                g = col.g();
                b = col.b();
            }
            
            offscreen_canvas->SetPixel(x, y, r, g, b);
        }
    }
    
    return true; // Continue rendering
}

void MyScene::register_properties() {
    add_property(speed);
    add_property(color);
    add_property(rainbow_mode);
}
```

### Scene Wrapper

```cpp
// In plugin main file
class MySceneWrapper : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override {
        return {new MyScene(), [](Scenes::Scene *scene) {
            delete scene;
        }};
    }
};

std::vector<std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>> 
MyAwesomePlugin::create_scenes() {
    std::vector<std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>> scenes;
    
    scenes.emplace_back(
        new MySceneWrapper(),
        [](Plugins::SceneWrapper *wrapper) { delete wrapper; }
    );
    
    return scenes;
}
```

## ⚙️ Properties System

The properties system provides automatic serialization, validation, and remote configuration.

### Property Types

```cpp
// Basic properties
PropertyPointer<int> count = MAKE_PROPERTY("count", int, 10);
PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 1.0f);
PropertyPointer<bool> enabled = MAKE_PROPERTY("enabled", bool, true);
PropertyPointer<std::string> text = MAKE_PROPERTY("text", std::string, "Hello");

// Color properties
PropertyPointer<rgb_matrix::Color> color = MAKE_PROPERTY("color", rgb_matrix::Color, rgb_matrix::Color(255, 0, 0));

// Properties with constraints
PropertyPointer<int> brightness = MAKE_PROPERTY_MINMAX("brightness", int, 50, 0, 100);
PropertyPointer<float> angle = MAKE_PROPERTY_MINMAX("angle", float, 0.0f, -180.0f, 180.0f);

// Required properties (must be set by user)
PropertyPointer<std::string> api_key = MAKE_PROPERTY_REQ("api_key", std::string, "");
```

### Property Registration

```cpp
void MyScene::register_properties() {
    add_property(speed);      // Register for automatic handling
    add_property(color);
    add_property(brightness);
    // Properties are automatically exposed via REST API
}
```

### Accessing Property Values

```cpp
bool MyScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    float current_speed = speed->get();           // Get current value
    auto current_color = color->get();            // Get color object
    uint8_t r = current_color.r();               // Extract RGB components
    
    // Use values in rendering...
    return true;
}
```

## 🖼️ Image Providers
Image Providers used (for now) in the ImageScene, where the user can specify if they want images to be displayed from for example a collection of pictures or from PixelJoint. By registering ImageProviders, you are able to add a new source of images to the led matrix.
A example for a image provider can be found [here](https://github.com/sshcrack/led-matrix/blob/master/plugins/PixelJoint/matrix/providers/collection.cpp). If you want to add a image provider yourself, you'll also need to edit the react-native app to support that provider.

## 🎭 Post-Processing Effects

Create screen-wide effects that modify the entire display.

```cpp
class FlashEffect : public Plugins::PostProcessingEffect {
    float intensity;
    float duration;
    float elapsed;
    
public:
    FlashEffect(float intensity, float duration) 
        : intensity(intensity), duration(duration), elapsed(0) {}
    
    bool process(rgb_matrix::RGBMatrixBase* matrix, 
                rgb_matrix::FrameCanvas* canvas) override {
        elapsed += 1.0f / 60.0f; // Assume 60 FPS
        
        if (elapsed >= duration) {
            return false; // Effect finished
        }
        
        // Apply flash effect
        float flash_amount = intensity * (1.0f - elapsed / duration);
        
        for (int y = 0; y < canvas->height(); y++) {
            for (int x = 0; x < canvas->width(); x++) {
                // Brighten pixels
                auto pixel = canvas->GetPixel(x, y);
                uint8_t r = std::min(255, (int)(pixel.r + flash_amount * 255));
                uint8_t g = std::min(255, (int)(pixel.g + flash_amount * 255));
                uint8_t b = std::min(255, (int)(pixel.b + flash_amount * 255));
                canvas->SetPixel(x, y, r, g, b);
            }
        }
        
        return true; // Continue effect
    }
    
    std::string get_name() const override {
        return "flash";
    }
};
```

## 🌐 REST API Integration

Add custom REST endpoints to your plugin.

```cpp
std::unique_ptr<router_t> MyAwesomePlugin::register_routes(std::unique_ptr<router_t> router) {
    // Add custom route
    router->http_get("/my_plugin/status", 
        [this](restinio::request_handle_t req, auto params) {
            auto resp = req->create_response();
            resp.set_body(R"({"status": "active", "version": "1.0"})");
            resp.header().content_type("application/json");
            return resp.done();
        });
    
    router->http_post("/my_plugin/trigger",
        [this](restinio::request_handle_t req, auto params) {
            // Handle POST request
            std::string body = req->body();
            // Process request...
            
            auto resp = req->create_response();
            resp.set_body(R"({"result": "success"})");
            resp.header().content_type("application/json");
            return resp.done();
        });
    
    return std::move(router);
}
```

## 💬 Desktop Communication

Communicate with the desktop application via WebSocket.

```cpp
// Handle WebSocket messages from desktop
void MyAwesomePlugin::on_websocket_message(const std::string &message) {
    if (message.starts_with("trigger:")) {
        // Handle trigger command
        send_msg_to_desktop("triggered");
    }
}

// Send initial message when desktop connects
std::optional<std::vector<std::string>> MyAwesomePlugin::on_websocket_open() {
    return std::vector<std::string>{"plugin_ready", "version:1.0"};
}

// Send data to desktop application
void MyAwesomePlugin::send_status_update() {
    nlohmann::json status;
    status["timestamp"] = get_current_time();
    status["active_scenes"] = get_active_scene_count();
    
    send_msg_to_desktop(status.dump());
}
```

## 🔧 Advanced Features

### Frame Timing

```cpp
class AnimatedScene : public Scene {
    FrameTimer frameTimer;
    
public:
    bool render(rgb_matrix::RGBMatrixBase *matrix) override {
        auto frame = frameTimer.tick();
        
        float dt = frame.dt;        // Delta time since last frame
        float t = frame.t;          // Total time since scene start
        uint32_t frame_num = frame.frame; // Frame counter
        
        // Use timing for smooth animations
        float phase = t * 2.0f * M_PI; // Complete cycle every second
        uint8_t brightness = (sin(phase) + 1.0f) * 127;
        
        // Render with timing...
        return true;
    }
};
```

### Resource Loading

```cpp
class ResourceScene : public Scene {
    std::string font_path;
    
public:
    void initialize(rgb_matrix::RGBMatrixBase *matrix, 
                   rgb_matrix::FrameCanvas *canvas) override {
        Scene::initialize(matrix, canvas);
        
        // Load plugin resources
        font_path = get_plugin_location() + "/fonts/my_font.bdf";
        
        // Initialize resources...
    }
};
```

### Lifecycle Hooks

```cpp
class LifecyclePlugin : public Plugins::BasicPlugin {
public:
    std::optional<std::string> before_server_init() override {
        // Initialize before server starts
        
    
    std::optional<std::string> after_server_init() override {
        // Called after server is ready
        start_background_tasks();
        return std::nullopt;
    }
    
    std::optional<std::string> pre_exit() override {
        // Cleanup before application exit
        cleanup_resources();
        return std::nullopt;
    }
};
```

## 📦 Building and Testing

### CMake Configuration

```cmake
# Basic plugin
register_plugin(
    MyPlugin
    matrix/MyPlugin.cpp
    matrix/scenes/Scene1.cpp
    matrix/scenes/Scene2.cpp
)

# Plugin with external dependencies and desktop sources (after the DESKTOP keyword)
register_plugin(
    AdvancedPlugin
    matrix/AdvancedPlugin.cpp
    matrix/scenes/AdvancedScene.cpp
    DESKTOP
    desktop/MyPlugin.cpp
    desktop/scenes/DesktopScene.cpp
    
)
target_link_libraries(AdvancedPlugin external_lib)
```

### Testing Your Plugin

1. **Build with emulator preset**:
   ```bash
   cmake --preset emulator
   cmake --preset emulator --build
   ```

2. **Run emulator**:
   ```bash
   ./run_emulator.sh
   ```

3. **Test REST API**:
   ```bash
   curl http://localhost:8080/list_scenes
   curl http://localhost:8080/my_plugin/status
   # You can also visit http://localhost:8080 in your web browser
   ```

## 🎯 Best Practices

### Performance
- Use `FrameTimer` for consistent animations (you can also use `wait_until_next_frame()` for rendering at a constant 60fps and use `set_target_fps` for modifying the FPS)
- Minimize memory allocations in render loops
- Cache expensive calculations
- Use efficient pixel access patterns

### Error Handling
- Validate property values in setters
- Handle missing resources gracefully
- Return meaningful error messages from lifecycle hooks

### Code Organization
- Keep scenes focused on single effects
- Use composition over inheritance
- Separate rendering logic from business logic
- Document public APIs thoroughly

### Configuration
- Provide sensible defaults for all properties
- Use appropriate min/max constraints
- Group related properties logically
- Consider user experience in property naming

## 🔍 Examples

### Study These Plugins

1. **[ExampleScenes](../plugins/ExampleScenes)** - Simple template and basic patterns
2. **[AudioVisualizer](../plugins/AudioVisualizer)** - Real-time data processing and visualization
3. **[WeatherOverview](../plugins/WeatherOverview)** - External API integration and data display
4. **[GameScenes](../plugins/GameScenes)** - Interactive content and game logic
5. **[FractalScenes](../plugins/FractalScenes)** - Mathematical visualizations and complex algorithms
6. **[SpotifyScenes](../plugins/SpotifyScenes)** - OAuth integration and music visualization
7. **[AmbientScenes](../plugins/AmbientScenes)** - Atmospheric effects and procedural generation
8. **[Shadertoy](../plugins/Shadertoy)** - Matrix & Desktop communication, pixel data streaming and custom UDP packet sending

### Quick Examples

See the [Examples section](#-examples) in the main documentation for complete working examples of:
- Basic color animation scenes
- Data-driven visualizations  
- Interactive game scenes
- External API integration
- Real-time audio processing

## 🆘 Getting Help

- **Documentation**: Check the main README and existing plugin code
- **Examples**: Study the included plugins for patterns and best practices
- **Community**: Join the project discussions and issue tracker
- **Debugging**: Use the emulator for development and testing

---

**Happy Plugin Development! 🚀**

Create amazing visual experiences and share them with the LED Matrix community!
