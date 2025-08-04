#include "RenderingDemoScene.h"
#include <cmath>
#include <algorithm>

using namespace Scenes;

struct RGB {
    uint8_t r, g, b;
    RGB(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) : r(r), g(g), b(b) {}
};

RenderingDemoScene::RenderingDemoScene() : rng(std::random_device{}()) {
    set_target_fps(60); // Smooth animations
}

void RenderingDemoScene::initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *canvas) {
    Scene::initialize(matrix, canvas);
    
    // Initialize particle system
    particles.clear();
    for (int i = 0; i < 20; i++) {
        Particle p;
        p.x = static_cast<float>(rng() % matrix_width);
        p.y = static_cast<float>(rng() % matrix_height);
        p.vx = (static_cast<float>(rng() % 200) - 100) / 100.0f;
        p.vy = (static_cast<float>(rng() % 200) - 100) / 100.0f;
        p.life = p.max_life = 3.0f + static_cast<float>(rng() % 300) / 100.0f;
        p.color = {static_cast<uint8_t>(rng() % 256), static_cast<uint8_t>(rng() % 256), static_cast<uint8_t>(rng() % 256)};
        particles.push_back(p);
    }
}

bool RenderingDemoScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    time += frame.dt * animation_speed->get();
    
    // Clear canvas
    offscreen_canvas->Clear();
    
    // Select demo based on mode
    switch (demo_mode->get()) {
        case 0:
            renderPixelManipulation();
            break;
        case 1:
            renderGeometricPatterns();
            break;
        case 2:
            renderColorInterpolation();
            break;
        case 3:
            renderParticleSystem();
            break;
        case 4:
            renderMathematicalVisualization();
            break;
        default:
            renderPixelManipulation();
            break;
    }
    
    return true;
}

void RenderingDemoScene::renderPixelManipulation() {
    // Demonstrate basic pixel manipulation techniques
    int width = matrix_width;
    int height = matrix_height;
    
    auto c1 = color1->get();
    auto c2 = color2->get();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Create a time-based pattern
            float pattern = sin(time + x * 0.2f) * cos(time * 0.7f + y * 0.15f);
            pattern = (pattern + 1.0f) / 2.0f; // Normalize to 0-1
            
            // Mix colors based on pattern
            uint8_t r = static_cast<uint8_t>(c1.r() * pattern + c2.r() * (1.0f - pattern));
            uint8_t g = static_cast<uint8_t>(c1.g() * pattern + c2.g() * (1.0f - pattern));
            uint8_t b = static_cast<uint8_t>(c1.b() * pattern + c2.b() * (1.0f - pattern));
            
            setPixelSafe(x, y, r, g, b);
        }
    }
}

void RenderingDemoScene::renderGeometricPatterns() {
    // Demonstrate drawing geometric shapes
    int center_x = matrix_width / 2;
    int center_y = matrix_height / 2;
    
    auto c1 = color1->get();
    auto c2 = color2->get();
    
    // Rotating circle
    float radius = 10 + 5 * sin(time);
    drawCircle(center_x, center_y, static_cast<int>(radius), c1.r(), c1.g(), c1.b());
    
    // Rotating lines
    for (int i = 0; i < 6; i++) {
        float angle = time + i * M_PI / 3;
        int x1 = center_x + static_cast<int>(cos(angle) * 15);
        int y1 = center_y + static_cast<int>(sin(angle) * 15);
        
        drawLine(center_x, center_y, x1, y1, c2.r(), c2.g(), c2.b());
    }
    
    // Corner squares
    int square_size = 3;
    for (int i = 0; i < 4; i++) {
        int x = (i % 2) * (matrix_width - square_size);
        int y = (i / 2) * (matrix_height - square_size);
        
        for (int dx = 0; dx < square_size; dx++) {
            for (int dy = 0; dy < square_size; dy++) {
                setPixelSafe(x + dx, y + dy, c1.r(), c1.g(), c1.b());
            }
        }
    }
}

void RenderingDemoScene::renderColorInterpolation() {
    // Demonstrate smooth color transitions
    auto c1 = color1->get();
    auto c2 = color2->get();
    RGB color1_rgb = {c1.r(), c1.g(), c1.b()};
    RGB color2_rgb = {c2.r(), c2.g(), c2.b()};
    
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            // Create different interpolation patterns
            float t1 = static_cast<float>(x) / matrix_width;
            float t2 = static_cast<float>(y) / matrix_height;
            float t3 = (sin(time + x * 0.1f) + 1.0f) / 2.0f;
            
            float t;
            if (smooth_animation->get()) {
                // Smooth interpolation
                t = (t1 + t2 + t3) / 3.0f;
            } else {
                // Hard transitions
                t = (static_cast<int>(t1 * 4) + static_cast<int>(t2 * 4)) % 2;
            }
            
            RGB result = interpolateColors(color1_rgb, color2_rgb, t);
            setPixelSafe(x, y, result.r, result.g, result.b);
        }
    }
}

void RenderingDemoScene::renderParticleSystem() {
    // Update particles
    for (auto& p : particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.life -= 0.016f; // Assume 60 FPS
        
        // Bounce off walls
        if (p.x <= 0 || p.x >= matrix_width - 1) p.vx = -p.vx;
        if (p.y <= 0 || p.y >= matrix_height - 1) p.vy = -p.vy;
        
        // Reset particle if dead
        if (p.life <= 0) {
            p.x = static_cast<float>(rng() % matrix_width);
            p.y = static_cast<float>(rng() % matrix_height);
            p.vx = (static_cast<float>(rng() % 200) - 100) / 100.0f;
            p.vy = (static_cast<float>(rng() % 200) - 100) / 100.0f;
            p.life = p.max_life;
            p.color = {static_cast<uint8_t>(rng() % 256), static_cast<uint8_t>(rng() % 256), static_cast<uint8_t>(rng() % 256)};
        }
    }
    
    // Render particles
    for (const auto& p : particles) {
        float alpha = p.life / p.max_life;
        uint8_t r = static_cast<uint8_t>(p.color.r * alpha);
        uint8_t g = static_cast<uint8_t>(p.color.g * alpha);
        uint8_t b = static_cast<uint8_t>(p.color.b * alpha);
        
        setPixelSafe(static_cast<int>(p.x), static_cast<int>(p.y), r, g, b);
        
        // Draw trail
        setPixelSafe(static_cast<int>(p.x - p.vx), static_cast<int>(p.y - p.vy), r/2, g/2, b/2);
    }
}

void RenderingDemoScene::renderMathematicalVisualization() {
    // Demonstrate mathematical functions for visualization
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            // Create complex mathematical pattern
            float fx = static_cast<float>(x) / matrix_width * 4 - 2;
            float fy = static_cast<float>(y) / matrix_height * 4 - 2;
            
            // Mandelbrot-like function
            float zx = fx, zy = fy;
            int iterations = 0;
            const int max_iterations = 20;
            
            while (iterations < max_iterations && (zx*zx + zy*zy) < 4) {
                float new_zx = zx*zx - zy*zy + fx + sin(time) * 0.3f;
                float new_zy = 2*zx*zy + fy + cos(time) * 0.3f;
                zx = new_zx;
                zy = new_zy;
                iterations++;
            }
            
            // Convert iterations to color
            float hue = (static_cast<float>(iterations) / max_iterations) * 360.0f + time * 50;
            uint8_t r, g, b;
            hsv_to_rgb(hue, 1.0f, iterations < max_iterations ? 1.0f : 0.0f, r, g, b);
            
            setPixelSafe(x, y, r, g, b);
        }
    }
}

void RenderingDemoScene::setPixelSafe(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
        offscreen_canvas->SetPixel(x, y, r, g, b);
    }
}

void RenderingDemoScene::drawCircle(int center_x, int center_y, int radius, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                setPixelSafe(center_x + x, center_y + y, r, g, b);
            }
        }
    }
}

void RenderingDemoScene::drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        setPixelSafe(x0, y0, r, g, b);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

RGB RenderingDemoScene::interpolateColors(const RGB& c1, const RGB& c2, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return {
        static_cast<uint8_t>(c1.r * (1.0f - t) + c2.r * t),
        static_cast<uint8_t>(c1.g * (1.0f - t) + c2.g * t),
        static_cast<uint8_t>(c1.b * (1.0f - t) + c2.b * t)
    };
}

void RenderingDemoScene::hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (s <= 0.0f) {
        r = g = b = static_cast<uint8_t>(v * 255);
        return;
    }

    h = fmod(h, 360.0f) / 60.0f;
    int hi = static_cast<int>(h);
    float f = h - hi;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (hi) {
        case 0: r = v * 255; g = t * 255; b = p * 255; break;
        case 1: r = q * 255; g = v * 255; b = p * 255; break;
        case 2: r = p * 255; g = v * 255; b = t * 255; break;
        case 3: r = p * 255; g = q * 255; b = v * 255; break;
        case 4: r = t * 255; g = p * 255; b = v * 255; break;
        default: r = v * 255; g = p * 255; b = q * 255; break;
    }
}

void RenderingDemoScene::register_properties() {
    add_property(demo_mode);
    add_property(animation_speed);
    add_property(color1);
    add_property(color2);
    add_property(smooth_animation);
}

std::unique_ptr<Scene, void (*)(Scene *)> RenderingDemoSceneWrapper::create() {
    return {new RenderingDemoScene(), [](Scene *scene) {
        delete scene;
    }};
}