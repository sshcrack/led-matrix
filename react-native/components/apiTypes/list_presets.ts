export interface ListPresets {
    [key: string]: RawPreset;
}

export interface RawPreset {
    scenes: Scene[];
}


export interface ImageArguments {
    begin: number;
    end: number;
}

export interface Scene {
    arguments: SceneArguments;
    type: string;
    uuid: string;
}

export interface SceneArguments {
    [key: string]: any;
}

export interface Preset {
    scenes: {
        [key: string]: Scene
    }
}

export function arrayToObjectPresets(preset: RawPreset): Preset {
    return {
        scenes: preset.scenes.reduce((acc, scene) => {
            acc[scene.uuid] = scene
            return acc
        }, {} as { [key: string]: Scene })
    }
}