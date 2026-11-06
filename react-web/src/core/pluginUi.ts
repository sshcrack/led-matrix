import type { ComponentType, ReactNode } from 'react'
import type { Property, ListScenes } from '~/apiTypes/list_scenes'
import type { Scene } from '~/apiTypes/list_presets'

/**
 * Web UI extension points for plugins.
 *
 * A plugin opts in by adding `plugins/<Name>/web/index.ts(x)` whose default
 * export is `definePluginUi({...})`. The web build discovers these files with
 * `import.meta.glob` (see pluginRegistry.ts) — there is nothing to register
 * centrally, no CMake change and no change to the other UI code. A plugin
 * without a `web/` folder keeps working: its scenes and properties are already
 * rendered from the metadata the matrix daemon sends via `/list_scenes`.
 *
 * Plugin web modules should import only from `react` and `~/…` so they work
 * identically in every UI variant.
 */

export interface PropertyControlProps<T = unknown> {
  property: Property<T>
  value: T
  onChange: (value: T) => void
  /** All arguments of the scene, for controls that depend on other properties. */
  args: Record<string, unknown>
}

export interface SceneActionContext {
  presetId: string
  scene: Scene
  definition: ListScenes
  navigate: (to: string) => void
}

export interface SceneAction {
  id: string
  label: string
  icon?: ReactNode
  /** Return false to hide the action for this scene. */
  when?: (ctx: SceneActionContext) => boolean
  run: (ctx: SceneActionContext) => void
}

export interface PresetTemplate {
  id: string
  name: string
  description: string
  /** Scene type names (as in /list_scenes). Missing scenes are skipped. */
  scenes: string[]
  /** Optional curated variant id per scene type. */
  variants?: Record<string, string>
}

export interface PluginUi {
  /** Matches the plugins/<Name>/ directory name. */
  plugin: string
  /** Extension API version this module was written against (default 1). Modules newer than the host are skipped. */
  uiApi?: number
  /** Custom controls keyed by the `control` id the scene declares in its property metadata. */
  propertyControls?: Record<string, ComponentType<PropertyControlProps<never>>>
  /** Extra buttons shown on every scene card/inspector that the plugin owns (or `when` matches). */
  sceneActions?: SceneAction[]
  /** One-tap starting points shown in the quick builder. */
  presetTemplates?: PresetTemplate[]
}

export const definePluginUi = (ui: PluginUi): PluginUi => ui
