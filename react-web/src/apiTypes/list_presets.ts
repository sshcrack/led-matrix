export interface ListPresets { [key: string]: RawPreset; }
export interface RawPreset { scenes: Scene[]; transition_duration?: number; transition_name?: string; }

export interface ImageArguments { begin: number; end: number; }

export interface Scene { arguments: SceneArguments; type: string; uuid: string; }
export interface SceneArguments { [key: string]: any; }

export interface Preset {
  transition_duration: number;
  transition_name: string;
  scenes: { [key: string]: Scene };
}

export function arrayToObjectPresets(preset: RawPreset): Preset {
  return {
    transition_duration: preset.transition_duration ?? 750,
    transition_name: preset.transition_name ?? 'blend',
    scenes: preset.scenes.reduce((acc, scene) => {
      acc[scene.uuid] = scene
      return acc
    }, {} as { [key: string]: Scene })
  }
}

export function objectToArrayPresets(preset: Preset): RawPreset {
  return {
    transition_duration: preset.transition_duration,
    transition_name: preset.transition_name,
    scenes: Object.values(preset.scenes)
  }
}
