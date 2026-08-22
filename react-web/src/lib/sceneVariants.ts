import type { ListScenes, SceneVariant } from '~/apiTypes/list_scenes'

export function sceneArgumentsForVariant(scene: ListScenes, variantId?: string): Record<string, unknown> {
  const defaults = Object.fromEntries(scene.properties.map(prop => [prop.name, prop.default_value]))
  if (!variantId) return defaults
  const variant = scene.descriptor?.variants.find(item => item.id === variantId)
  return variant ? { ...defaults, ...variant.properties } : defaults
}

export function sceneVariant(scene: ListScenes, variantId?: string): SceneVariant | undefined {
  return variantId ? scene.descriptor?.variants.find(item => item.id === variantId) : undefined
}

export function sceneDisplayName(name: string): string {
  return name.replaceAll('_', ' ').replaceAll('-', ' ').replace(/\b\w/g, letter => letter.toUpperCase())
}

export function intensityLabel(value = 0.5): string {
  if (value < 0.34) return 'Calm'
  if (value < 0.68) return 'Balanced'
  return 'Energetic'
}

export function costLabel(value = 0.5): string {
  if (value < 0.34) return 'Light'
  if (value < 0.68) return 'Moderate'
  return 'Heavy'
}
