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

/** Web-UI extension API version this build implements. Bump on breaking changes to PluginUi. */
export const UI_API_VERSION = 1

/** Discover plugins/<Name>/web/index.ts(x). Each is its own lazy chunk, fetched only when needed. */
const modules = import.meta.glob<{ default: PluginUi }>('../../../plugins/*/web/index.{ts,tsx}')

export const pluginNameFromPath = (path: string) => /plugins\/([^/]+)\/web\//.exec(path)?.[1] ?? path

interface PluginUiInfo { ui_api_version?: number; plugins?: string[] }

/** Which discovered web modules to load, given what the daemon reports (null = unknown, load all). */
export function selectPluginModules(paths: string[], info: PluginUiInfo | null): string[] {
  if (!info?.plugins) return paths
  if ((info.ui_api_version ?? 1) > UI_API_VERSION) return []   // daemon is newer than this UI build
  const installed = new Set(info.plugins)
  return paths.filter(p => installed.has(pluginNameFromPath(p)))
}

/**
 * Ask the daemon (`/plugin_ui`) which plugins are installed and load only the
 * matching web modules. An older daemon without the route, or a failed
 * request, falls back to loading every bundled module (they're harmless:
 * a control that no scene declares is never rendered).
 */
export async function loadPluginUis(apiUrl: string) {
  let info: PluginUiInfo | null = null
  try {
    const res = await fetch(`${apiUrl}/plugin_ui`)
    if (res.ok) info = await res.json()
  } catch { /* old daemon / offline: load all */ }

  await Promise.all(selectPluginModules(Object.keys(modules), info).map(async path => {
    try {
      const ui = (await modules[path]()).default
      if ((ui.uiApi ?? 1) > UI_API_VERSION) {
        console.warn(`[plugin-ui] ${ui.plugin} needs UI API ${ui.uiApi}, this build has ${UI_API_VERSION}`)
        return
      }
      registerPluginUi(ui)
    } catch (e) { console.error(`[plugin-ui] failed to load ${path}`, e) }
  }))
}
