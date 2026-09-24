import type { ListScenes } from '~/apiTypes/list_scenes'

/** Server-side preference values; absence means "follow the look's default". */
export type AutomaticPreference = 'favorite' | 'on' | 'hidden'
export type AutomaticPreferences = Record<string, AutomaticPreference>
/** What the user sees and picks for a look. */
export type AutomaticState = 'favorite' | 'on' | 'off'

export interface AutomaticLook {
  key: string
  label: string
  description?: string
  defaultOn: boolean
  state: AutomaticState
}

/** Must match AutomaticDirector::look_key on the matrix. */
export function lookKey(sceneName: string, variantId?: string) {
  return variantId ? `${sceneName}/${variantId}` : sceneName
}

/** Automatic Mode plays one instance per curated variant, or the scene itself when it has none. */
export function automaticLooks(scene: ListScenes, preferences: AutomaticPreferences): AutomaticLook[] {
  const descriptor = scene.descriptor
  if (!descriptor?.automatic_eligible) return []
  const sceneDefault = descriptor.automatic_default ?? true
  const looks = descriptor.variants.length > 0
    ? descriptor.variants.map(variant => ({
        key: lookKey(scene.name, variant.id),
        label: variant.label,
        description: variant.description,
        defaultOn: sceneDefault && (variant.automatic_default ?? true),
      }))
    : [{ key: lookKey(scene.name), label: 'Scene', description: undefined, defaultOn: sceneDefault }]
  return looks.map(look => ({ ...look, state: stateFor(preferences[look.key], look.defaultOn) }))
}

function stateFor(preference: AutomaticPreference | undefined, defaultOn: boolean): AutomaticState {
  if (preference === 'favorite') return 'favorite'
  if (preference === 'hidden') return 'off'
  if (preference === 'on') return 'on'
  return defaultOn ? 'on' : 'off'
}

/** The request value that makes `look` reach `state`, preferring "default" so presets stay minimal. */
export function preferenceFor(look: AutomaticLook, state: AutomaticState): AutomaticPreference | 'default' {
  if (state === 'favorite') return 'favorite'
  if (state === 'on') return look.defaultOn ? 'default' : 'on'
  return look.defaultOn ? 'hidden' : 'default'
}
