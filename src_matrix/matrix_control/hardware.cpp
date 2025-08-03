#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/post_processor.h"

#include <csignal>
#if !defined(ENABLE_EMULATOR) && defined(MOTION_SENSOR)
#include <wiringPi.h>
#endif
#include <chrono>
#include "spdlog/spdlog.h"

using namespace rgb_matrix;
using namespace spdlog;

// Motion sensor constants
constexpr int MOTION_SENSOR_PIN = 14;
constexpr unsigned long MOTION_TIMEOUT_MS = 300000; // 5 minutes in milliseconds
unsigned long last_motion_time = 0;
bool motion_detected = false;

void hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix) {
    info("Press Ctrl+C to quit");

    FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
    string last_scheduled_preset = "";
    
    // Initialize global post-processor
    if (!global_post_processor) {
        global_post_processor = new PostProcessor();
        spdlog::info("Post-processor initialized");
    }
    
    while (!interrupt_received) {
        // Check for active scheduled preset
        if (config->is_scheduling_enabled()) {
            auto active_preset = config->get_active_scheduled_preset();
            if (active_preset.has_value() && active_preset.value() != last_scheduled_preset) {
                debug("Switching to scheduled preset: {}", active_preset.value());
                config->set_curr(active_preset.value());
                last_scheduled_preset = active_preset.value();
            } else if (!active_preset.has_value() && !last_scheduled_preset.empty()) {
                debug("No active schedule, clearing scheduled preset");
                last_scheduled_preset = "";
            }
        }

#if !defined(ENABLE_EMULATOR) && defined(MOTION_SENSOR)
        // Check motion sensor
        int sensor_state = digitalRead(MOTION_SENSOR_PIN);
        unsigned long current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
            
        if (sensor_state == HIGH) {
            // Motion detected, update the timestamp
            last_motion_time = current_time;
            
            if (turned_off) {
                // Turn on the canvas if it was off
                debug("Motion detected, turning on canvas");
                turned_off = false;
            }
        } else if (!turned_off && (current_time - last_motion_time > MOTION_TIMEOUT_MS)) {
            // No motion for 5 minutes, turn off the canvas
            debug("No motion for 5 minutes, turning off canvas");
            turned_off = true;
        }

#endif
        if (!turned_off) {
            offscreen_canvas = update_canvas(matrix, offscreen_canvas);
            exit_canvas_update = false;
        }

        while (turned_off) {
            matrix->Clear();
            SleepMillis(1000);

#if !defined(ENABLE_EMULATOR) && defined(MOTION_SENSOR)
            // Check the sensor while turned off
            if (digitalRead(MOTION_SENSOR_PIN) == HIGH) {
                debug("Motion detected, turning on canvas");
                last_motion_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                turned_off = false;
                break;
            }

#endif
        }
    }

    // Cleanup post-processor
    if (global_post_processor) {
        delete global_post_processor;
        global_post_processor = nullptr;
        spdlog::info("Post-processor cleaned up");
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    info("Finished, shutting down...");
}

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix) {

    Constants::height = matrix->height();
    Constants::width = matrix->width();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);
    
    // Initialize GPIO for motion sensor
#if !defined(ENABLE_EMULATOR) && defined(MOTION_SENSOR)
    wiringPiSetup();
    pinMode(MOTION_SENSOR_PIN, INPUT);
#endif
    debug("Motion sensor initialized on pin {}", MOTION_SENSOR_PIN);
    
    // Initialize motion timer
    last_motion_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    debug("Running hardware mainloop...");
    hardware_mainloop(matrix);
    return 0;
}