import { v4 as uuidv4 } from 'uuid'
import type { ListProviders, ListScenes, Property, ProviderValue } from '~/apiTypes/list_scenes'

/**
 * Scene sources: the image / shader providers a scene draws from (PixelJoint `image_scene`,
 * Shadertoy `shadertoy`). The daemon stores them in one json property as an array of
 * `{type, arguments}` (`providers` / `shader_providers`); the editor adds a local `uuid` per item
 * as a React key and strips it again on write.
 */
export type SourceKind = 'image' | 'shader'

/** The `control` id a scene declares on its sources property (`.control("…")` in C++). */
export const SOURCE_CONTROLS: Record<SourceKind, string> = { image: 'image_sources', shader: 'shader_sources' }
/** Daemons from before the control ids: recognise the property by its name. */
const LEGACY_NAMES: Record<SourceKind, string> = { image: 'providers', shader: 'shader_providers' }

export function sourcesProperty(definition: ListScenes | undefined): { kind: SourceKind; property: Property } | null {
  for (const property of definition?.properties ?? []) {
    if (property.type_id !== 'json') continue
    for (const kind of ['image', 'shader'] as const) {
      const control = property.additional?.control
      if (control === SOURCE_CONTROLS[kind] || (!control && property.name === LEGACY_NAMES[kind])) return { kind, property }
    }
  }
  return null
}

/** Argument names the UI once wrote for the pages provider; the daemon reads `pages_begin` / `pages_end`. */
const migrateArguments = (type: string, args: Record<string, unknown>): Record<string, unknown> => {
  if (type !== 'pages' || !('begin' in args || 'end' in args)) return args
  const { begin, end, ...rest } = args
  return { pages_begin: begin ?? 0, pages_end: end ?? -1, ...rest }
}

export function readSources(stored: unknown): ProviderValue[] {
  if (!Array.isArray(stored)) return []
  return stored
    .filter((s): s is { type: string; arguments?: Record<string, unknown> } => !!s && typeof s === 'object' && typeof s.type === 'string')
    .map(s => ({ type: s.type, uuid: uuidv4(), arguments: migrateArguments(s.type, { ...(s.arguments ?? {}) }) }) as ProviderValue)
}

export const writeSources = (sources: ProviderValue[]) => sources.map(({ type, arguments: args }) => ({ type, arguments: args }))

export interface SourceOption { value: string; label: string; description: string; defaults: Record<string, unknown> }

const KNOWN: Record<SourceKind, Omit<SourceOption, 'defaults'>[]> = {
  image: [
    { value: 'collection', label: 'Image collection', description: 'Select multiple uploaded images as one source.' },
    { value: 'pages', label: 'Page range', description: 'Use a contiguous range from the image library.' },
  ],
  shader: [
    { value: 'random', label: 'Random shader', description: 'Pick a shader from a configurable page range.' },
    { value: 'shader_collection', label: 'Shader collection', description: 'Use a curated set of shader URLs.' },
  ],
}
const KNOWN_DEFAULTS: Record<string, Record<string, unknown>> = {
  collection: { images: [] },
  pages: { pages_begin: 0, pages_end: -1 },
  random: { min_page: 0, max_page: 100 },
  shader_collection: { urls: [] },
}

/** One option per provider type: the daemon's providers with their defaults, friendly labels where known. */
export function sourceOptions(kind: SourceKind, daemon: ListProviders[] | null | undefined): SourceOption[] {
  const known = KNOWN[kind]
  const byName = new Map((daemon ?? []).map(p => [p.name, p]))
  const fromDaemon = (name: string) => {
    const def = byName.get(name)
    return def ? Object.fromEntries(def.properties.map(p => [p.name, p.default_value])) : undefined
  }
  // Without a daemon list, offer the built-in types; with one, only what the daemon has.
  const options: SourceOption[] = known
    .filter(k => !daemon || byName.has(k.value))
    .map(k => ({ ...k, defaults: fromDaemon(k.value) ?? KNOWN_DEFAULTS[k.value] ?? {} }))
  for (const def of daemon ?? []) {
    if (known.some(k => k.value === def.name)) continue
    options.push({ value: def.name, label: def.name, description: `${def.properties.length} configurable options`, defaults: fromDaemon(def.name) ?? {} })
  }
  return options
}

export const newSource = (option: SourceOption): ProviderValue =>
  ({ type: option.value, uuid: uuidv4(), arguments: structuredClone(option.defaults) }) as ProviderValue

/** The daemon route listing the provider types for a kind. */
export const providersRoute = (kind: SourceKind) => kind === 'shader' ? '/list_shader_providers' : '/list_providers'
