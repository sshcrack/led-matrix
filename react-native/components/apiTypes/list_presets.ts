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
    [key: string]: any;
}
