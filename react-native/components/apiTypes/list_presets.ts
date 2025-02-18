export interface ListPresets {
    [key: string]: Preset;
}

export interface Preset {
    scenes: Scene[];
}


export interface ImageArguments {
    begin: number;
    end:   number;
}

export interface Scene {
    arguments: SceneArguments;
    type:      string;
    uuid: string;
}

export interface SceneArguments {
    [key: string]: any;
}
