import { useSyncExternalStore } from 'react'
import { createElement } from 'react'
import { Link2 } from 'lucide-react'
import type { PluginUi, PropertyControlProps, SceneAction, PresetTemplate } from './pluginUi'
import type { ComponentType } from 'react'
import { installPluginSdk } from './pluginSdk'
import { sourcesProperty } from './sceneSources'

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
    when: ({ definition }) => sourcesProperty(definition) !== null,
    run: ({ presetId, scene, definition, navigate }) => {
      const shader = sourcesProperty(definition)?.kind === 'shader'
      navigate(`/${shader ? 'modify-shader-providers' : 'modify-providers'}/${encodeURIComponent(presetId)}/${scene.uuid}`)
    },
  }],
})

/** Web-UI extension API version this build implements. Bump on breaking changes to PluginUi. */
export const UI_API_VERSION = 1

/** Discover plugins/<Name>/web/index.ts(x). Each is its own lazy chunk, fetched only when needed. */
const modules = import.meta.glob<{ default: PluginUi }>('../../../plugins/*/web/index.{ts,tsx}')

export const pluginNameFromPath = (path: string) => /plugins\/([^/]+)\/web\//.exec(path)?.[1] ?? path

export interface RuntimeUi { api_version?: number; js: string; css?: string }
export interface PluginUiInfo { ui_api_version?: number; plugins?: string[]; web_ui?: Record<string, RuntimeUi> }

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
 * What to load: runtime bundles where the plugin ships a compatible one, bundled
 * modules for the other installed plugins. A plugin whose installed bundle needs
 * a newer UI API than this build is skipped entirely — its older bundled module
 * would otherwise resurrect controls the upgraded plugin may have dropped.
 */
export function planPluginUis(paths: string[], info: PluginUiInfo | null) {
  const runtime = selectRuntimeUis(info)
  const skip = new Set(runtime.map(([n]) => n))
  for (const [name, ui] of Object.entries(info?.web_ui ?? {})) if ((ui.api_version ?? 1) > UI_API_VERSION) skip.add(name)
  const bundled = selectPluginModules(paths, info).filter(p => !skip.has(pluginNameFromPath(p)))
  return { runtime, bundled }
}

/**
 * A plugin's ui.css is plain Tailwind utilities, injected after the host CSS. Its unconditional
 * `.hidden`/`.flex` rules would then beat the host's responsive `.lg:flex` (same specificity, later),
 * hiding e.g. the whole navbar. Utilities the host already defines are identical, so drop those
 * top-level rules from the plugin sheet; classes only the plugin uses (and its media variants) stay.
 */
function dropHostDuplicateRules(style: HTMLStyleElement) {
  const sheet = style.sheet
  if (!sheet) return
  try {
    const host = new Set<string>()
    for (const s of Array.from(document.styleSheets)) {
      // Host sheets: <link> in production, inline <style> under the Vite dev server. Skip plugin sheets.
      if (s === sheet || (s.ownerNode instanceof HTMLStyleElement && s.ownerNode.dataset.pluginUi !== undefined)) continue
      for (const r of Array.from(s.cssRules)) if (r instanceof CSSStyleRule) host.add(r.selectorText)
    }
    for (let i = sheet.cssRules.length - 1; i >= 0; i--) {
      const r = sheet.cssRules[i]
      if (r instanceof CSSStyleRule && host.has(r.selectorText)) sheet.deleteRule(i)
    }
  } catch { /* cross-origin stylesheet: keep everything */ }
}

/** A loaded plugin web module plus the stylesheet that belongs to it (runtime bundles only). */
export interface LoadedPluginUi { ui: PluginUi; css?: string }

/** Where plugin web modules come from. Production: the daemon + this build's bundled modules. */
export interface PluginUiSources {
  /** `/plugin_ui`, or null when the daemon cannot say (old daemon, offline). */
  info(): Promise<PluginUiInfo | null>
  /** Paths of the modules bundled into this build. */
  bundledPaths(): string[]
  bundled(path: string): Promise<LoadedPluginUi>
  /** Fetch and evaluate a plugin's installed bundle. Nothing is applied to the page here. */
  runtime(name: string, ui: RuntimeUi): Promise<LoadedPluginUi>
}

/**
 * Fetch a plugin's installed bundle through the (authenticated) REST API and evaluate it from a
 * Blob URL. Going through fetch + Blob instead of a direct `import(url)` keeps the API token/cookie
 * handling and works behind the dev proxy.
 */
export function daemonPluginUiSources(apiUrl: string): PluginUiSources {
  return {
    async info() {
      try {
        const res = await fetch(`${apiUrl}/plugin_ui`)
        return res.ok ? await res.json() as PluginUiInfo : null
      } catch { return null }
    },
    bundledPaths: () => Object.keys(modules),
    bundled: async path => ({ ui: (await modules[path]()).default }),
    async runtime(name, ui) {
      const get = async (file: string) => {
        const res = await fetch(`${apiUrl}/plugin_ui/${encodeURIComponent(name)}/${file}`)
        if (!res.ok) throw new Error(`${file}: HTTP ${res.status}`)
        return res.text()
      }
      const [js, css] = await Promise.all([get(ui.js), ui.css ? get(ui.css) : Promise.resolve('')])
      const url = URL.createObjectURL(new Blob([js], { type: 'text/javascript' }))
      try { return { ui: (await import(/* @vite-ignore */ url) as { default: PluginUi }).default, css: css || undefined } }
      finally { URL.revokeObjectURL(url) }
    },
  }
}

/** Replace the stylesheet owned by `plugin` (none when `css` is empty). */
function applyPluginCss(plugin: string, css: string | undefined) {
  for (const el of Array.from(document.head.querySelectorAll<HTMLStyleElement>('style[data-plugin-ui]')))
    if (el.dataset.pluginUi === plugin) el.remove()
  if (!css) return
  const style = document.createElement('style')
  style.dataset.pluginUi = plugin
  style.textContent = css
  document.head.appendChild(style)
  dropHostDuplicateRules(style)
}

/**
 * Activates the plugin web UIs once. Asks the daemon (`/plugin_ui`) which plugins are installed;
 * a plugin that ships a compatible installed bundle uses it (UI updates need no web rebuild),
 * falling back to the module bundled into this build if the bundle fails; other installed plugins
 * use their bundled module. An older daemon without the route, or a failed request, loads every
 * bundled module (a control no scene declares is never rendered).
 *
 * A plugin's registration and its stylesheet are applied together, after the module evaluated and
 * passed the API check: a failed or rejected load leaves no CSS behind. Calling `activate` again
 * returns the same activation.
 */
export function createPluginActivation(sources: PluginUiSources, register: (ui: PluginUi) => void = registerPluginUi) {
  let activation: Promise<void> | null = null

  const accept = ({ ui, css }: LoadedPluginUi, source: string) => {
    if ((ui.uiApi ?? 1) > UI_API_VERSION) throw new Error(`${ui.plugin} (${source}) needs UI API ${ui.uiApi}, this build has ${UI_API_VERSION}`)
    applyPluginCss(ui.plugin, css)
    register(ui)
  }

  async function run() {
    installPluginSdk()
    const paths = sources.bundledPaths()
    const { runtime, bundled } = planPluginUis(paths, await sources.info())
    const loadBundled = async (path: string) => {
      try { accept(await sources.bundled(path), 'bundled') }
      catch (e) { console.error(`[plugin-ui] failed to load ${path}`, e) }
    }
    await Promise.all([
      ...runtime.map(async ([name, ui]) => {
        try { accept(await sources.runtime(name, ui), 'runtime') }
        catch (e) {
          console.error(`[plugin-ui] runtime bundle of ${name} failed, trying the bundled one`, e)
          const path = paths.find(p => pluginNameFromPath(p) === name)
          if (path) await loadBundled(path)
        }
      }),
      ...bundled.map(loadBundled),
    ])
  }

  return { activate: () => activation ??= run() }
}

let appActivation: ReturnType<typeof createPluginActivation> | null = null
/** The app's activation (MatrixProvider). Idempotent: StrictMode double effects load once. */
export function loadPluginUis(apiUrl: string) {
  appActivation ??= createPluginActivation(daemonPluginUiSources(apiUrl))
  return appActivation.activate()
}
