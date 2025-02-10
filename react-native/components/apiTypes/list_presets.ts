export interface ListPresets {
    [key: string]: Preset;
}

export interface Preset {
    images: Image[];
    scenes: Scene[];
}

export interface Image {
    arguments: ImageArguments;
    type:      string;
}

export interface ImageArguments {
    begin: number;
    end:   number;
}

export interface Scene {
    arguments: SceneArguments;
    type:      string;
}

export interface SceneArguments {
    duration:              number;
    weight:                number;
    acceleration?:         number;
    bounce?:               number;
    delay_ms?:             number;
    numParticles?:         number;
    shake?:                number;
    velocity?:             number;
    ball_size?:            number;
    ball_speed?:           number;
    max_speed_multiplier?: number;
    paddle_height?:        number;
    paddle_speed?:         number;
    paddle_width?:         number;
    speed_multiplier?:     number;
    target_fps?:           number;
    fall_speed_ms?:        number;
    enable_twinkle?:       boolean;
    max_depth?:            number;
    num_stars?:            number;
    speed?:                number;
    color_speed?:          number;
    move_range?:           number;
    num_blobs?:            number;
    threshold?:            number;
    fps?:                  number;
}
