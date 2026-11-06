import { useSyncExternalStore } from 'react'
import { createElement } from 'react'
import { Link2 } from 'lucide-react'
import type { PluginUi, PropertyControlProps, SceneAction, PresetTemplate } from './pluginUi'
import type { ComponentType } from 'react'

const registered = new Map<string, PluginUi>()
const listeners = new Set<() => void>()
let snapshot: PluginUi[] = []

export function registerPluginUi(ui: PluginUi) {
  registered.set(ui.plugin, ui)
  snapshot = [...registered.values()]
  listeners.forEach(l => l())
}

const subscribe = (l: () => void) => { listeners.add(l); return () => { listeners.delete(l) } }
export const usePluginUis = () => useSyncExternalStore(subscribe, () => snapshot)

export function findPropertyControl(uis: PluginUi[], control?: string): ComponentType<PropertyControlProps<never>> | undefined {
  if (!control) return undefined
  for (const ui of uis) if (ui.propertyControls?.[control]) return ui.propertyControls[control]
}

export const collectSceneActions = (uis: PluginUi[]): SceneAction[] => uis.flatMap(u => u.sceneActions ?? [])
export const collectTemplates = (uis: PluginUi[]): PresetTemplate[] => uis.flatMap(u => u.presetTemplates ?? [])

/**
 * Built-in extensions use exactly the same API as third-party plugin modules.
 * The "Configure sources" route used to be hard-coded inside the scene card.
 */
registerPluginUi({
  plugin: '@core/sources',
  sceneActions: [{
    id: 'configure-sources',
    label: 'Configure sources',
    icon: createElement(Link2, { className: 'h-4 w-4' }),
    when: ({ definition }) => definition.properties.some(p => p.type_id === 'json'
      && (p.additional?.provider_type === 'image' || p.additional?.provider_type === 'shader')),
    run: ({ presetId, scene, definition, navigate }) => {
      const shader = definition.properties.some(p => p.type_id === 'json' && p.additional?.provider_type === 'shader')
      navigate(`/${shader ? 'modify-shader-providers' : 'modify-providers'}/${encodeURIComponent(presetId)}/${scene.uuid}`)
    },
  }],
})

/** Discover plugins/<Name>/web/index.ts(x). Lazy chunks: only fetched when the app boots. */
const modules = import.meta.glob<{ default: PluginUi }>('../../../plugins/*/web/index.{ts,tsx}')
export async function loadPluginUis() {
  await Promise.all(Object.entries(modules).map(async ([path, load]) => {
    try { registerPluginUi((await load()).default) }
    catch (e) { console.error(`[plugin-ui] failed to load ${path}`, e) }
  }))
}
