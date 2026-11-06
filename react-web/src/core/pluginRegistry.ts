import { useSyncExternalStore } from 'react'
import { createElement } from 'react'
import { Link2 } from 'lucide-react'
import type { PluginUi, PropertyControlProps, SceneAction, PresetTemplate } from './pluginUi'
import type { ComponentType } from 'react'
import { installPluginSdk } from './pluginSdk'

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

interface RuntimeUi { api_version?: number; js: string; css?: string }
interface PluginUiInfo { ui_api_version?: number; plugins?: string[]; web_ui?: Record<string, RuntimeUi> }

/** Which discovered (build-time) web modules to load, given what the daemon reports (null = unknown, load all). */
export function selectPluginModules(paths: string[], info: PluginUiInfo | null): string[] {
  if (!info?.plugins) return paths
  if ((info.ui_api_version ?? 1) > UI_API_VERSION) return []   // daemon is newer than this UI build
  const installed = new Set(info.plugins)
  return paths.filter(p => installed.has(pluginNameFromPath(p)))
}

/** Plugins whose own installed UI bundle should be loaded at runtime, and the rest left to bundled modules. */
export function selectRuntimeUis(info: PluginUiInfo | null): [string, RuntimeUi][] {
  if (!info?.web_ui || (info.ui_api_version ?? 1) > UI_API_VERSION) return []
  const installed = new Set(info.plugins ?? [])
  return Object.entries(info.web_ui).filter(([name, ui]) => installed.has(name) && (ui.api_version ?? 1) <= UI_API_VERSION)
}

/**
 * Fetch a plugin's installed bundle through the (authenticated) REST API and
 * evaluate it from a Blob URL. Going through fetch + Blob instead of a direct
 * `import(url)` keeps the API token/cookie handling and works when the UI is
 * served from a different origin than the daemon (dev proxy).
 */
async function loadRuntimeUi(apiUrl: string, name: string, ui: RuntimeUi): Promise<PluginUi> {
  const get = async (file: string) => {
    const res = await fetch(`${apiUrl}/plugin_ui/${encodeURIComponent(name)}/${file}`)
    if (!res.ok) throw new Error(`${file}: HTTP ${res.status}`)
    return res.text()
  }
  const [js, css] = await Promise.all([get(ui.js), ui.css ? get(ui.css) : Promise.resolve('')])
  if (css) {
    const style = document.createElement('style')
    style.dataset.pluginUi = name
    style.textContent = css
    document.head.appendChild(style)
  }
  const url = URL.createObjectURL(new Blob([js], { type: 'text/javascript' }))
  try { return (await import(/* @vite-ignore */ url) as { default: PluginUi }).default }
  finally { URL.revokeObjectURL(url) }
}

/**
 * Ask the daemon (`/plugin_ui`) which plugins are installed and register their
 * web UIs. A plugin that ships an installed runtime bundle uses it (so UI
 * updates need no web rebuild); otherwise the module bundled into this build is
 * used. An older daemon without the route, or a failed request, falls back to
 * loading every bundled module (a control no scene declares is never rendered).
 */
export async function loadPluginUis(apiUrl: string) {
  let info: PluginUiInfo | null = null
  try {
    const res = await fetch(`${apiUrl}/plugin_ui`)
    if (res.ok) info = await res.json()
  } catch { /* old daemon / offline: load all */ }

  const register = (ui: PluginUi, source: string) => {
    if ((ui.uiApi ?? 1) > UI_API_VERSION) {
      console.warn(`[plugin-ui] ${ui.plugin} (${source}) needs UI API ${ui.uiApi}, this build has ${UI_API_VERSION}`)
      return
    }
    registerPluginUi(ui)
  }

  installPluginSdk()
  const runtime = selectRuntimeUis(info)
  const runtimeNames = new Set(runtime.map(([n]) => n))
  const bundled = selectPluginModules(Object.keys(modules), info).filter(p => !runtimeNames.has(pluginNameFromPath(p)))

  await Promise.all([
    ...runtime.map(async ([name, ui]) => {
      try { register(await loadRuntimeUi(apiUrl, name, ui), 'runtime') }
      catch (e) {
        console.error(`[plugin-ui] runtime bundle of ${name} failed, trying the bundled one`, e)
        const path = Object.keys(modules).find(p => pluginNameFromPath(p) === name)
        if (path) try { register((await modules[path]()).default, 'bundled') } catch (e2) { console.error(`[plugin-ui] ${name} bundled fallback failed`, e2) }
      }
    }),
    ...bundled.map(async path => {
      try { register((await modules[path]()).default, 'bundled') }
      catch (e) { console.error(`[plugin-ui] failed to load ${path}`, e) }
    }),
  ])
}
