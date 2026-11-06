import { v4 as uuidv4 } from 'uuid'
import type { ListScenes } from '~/apiTypes/list_scenes'
import type { RawPreset, Scene } from '~/apiTypes/list_presets'
import { sceneArgumentsForVariant, sceneDisplayName } from '~/lib/sceneVariants'

/** A fresh scene instance using the curated default look when the scene has one. */
export function newSceneInstance(def: ListScenes, variantId?: string): Scene {
  const variant = variantId ?? def.descriptor?.variants.find(v => v.automatic_default)?.id
  return { uuid: uuidv4(), type: def.name, arguments: sceneArgumentsForVariant(def, variant), variant: variant || undefined }
}

/** "Aurora", "Aurora + Fire", "Aurora + 3 more" */
export function suggestPresetName(names: string[]): string {
  const pretty = names.map(n => sceneDisplayName(n.replace(/^[A-Za-z]+:/, '')))
  if (pretty.length === 0) return 'New preset'
  if (pretty.length === 1) return pretty[0]
  if (pretty.length === 2) return `${pretty[0]} + ${pretty[1]}`
  return `${pretty[0]} + ${pretty.length - 1} more`
}

export function presetThumbNames(preset: RawPreset, defs: ListScenes[], max = 3): string[] {
  const withPreview = new Set(defs.filter(d => d.has_preview).map(d => d.name))
  return preset.scenes.map(s => s.type).filter(t => withPreview.has(t)).slice(0, max)
}

export function scenePreviewUrl(apiUrl: string | null, name: string) {
  return `${apiUrl ?? ''}/scene_preview?name=${encodeURIComponent(name)}`
}
