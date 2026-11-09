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

/** Automatic Mode plays one instance per curated variant, or the scene itself when it has none. Scenes that follow the real world also play their default look. */
export function automaticLooks(scene: ListScenes, preferences: AutomaticPreferences): AutomaticLook[] {
  const descriptor = scene.descriptor
  if (!descriptor?.automatic_eligible) return []
  const sceneDefault = descriptor.automatic_default ?? true
  const variantLooks = descriptor.variants.map(variant => ({
    key: lookKey(scene.name, variant.id),
    label: variant.label,
    description: variant.description,
    defaultOn: sceneDefault && (variant.automatic_default ?? true),
  }))
  const followsRealWorld = descriptor.tags.includes('environment-adaptive')
  const looks = variantLooks.length > 0 && !followsRealWorld
    ? variantLooks
    : [
        {
          key: lookKey(scene.name),
          label: followsRealWorld ? 'Follows the real world' : 'Scene',
          description: followsRealWorld ? 'Time of day, weather and season follow the real world outside.' : undefined,
          defaultOn: sceneDefault,
        },
        ...variantLooks,
      ]
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
