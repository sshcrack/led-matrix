#pragma once

namespace WeatherElements {
    struct Star {
        int x, y;
        uint8_t brightness;
        float twinkle_speed;
        float twinkle_phase;
    };

    struct Raindrop {
        float x, y;
        float speed;
        int length;
        int alpha;
    };

    struct Snowflake {
        float x, y;
        float speed;
        float drift;
        int size;
        float angle;
        float rotation_speed;
    };

    struct Cloud {
        float x, y;
        float speed;
        int width;
        int height;
        int opacity;
    };

    struct LightningBolt {
        float x;
        int start_time;
        int duration;
        bool active;
    };
}
