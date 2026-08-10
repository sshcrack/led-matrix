export interface ScenePreviewSpec {
  enabled: boolean
  inputs: string[]
  property_overrides: Record<string, unknown>
}

export interface SceneCapabilities {
  requires_desktop: boolean
  requires_audio: boolean
  requires_network: boolean
  interactive: boolean
  can_generate_preview: boolean
  supports_audio: boolean
  music_director_eligible: boolean
}

export interface PropertyUiMetadata {
  label?: string
  description?: string
  group?: string
  unit?: string
  control?: string
  step?: number
  presets?: unknown[]
  advanced?: boolean
  visible_if?: { property: string; equals: unknown }
  min?: number
  max?: number
  enum_name?: string
  enum_values?: Array<{ value: string; display_name?: string }>
  values?: unknown[]
  [key: string]: unknown
}

export interface ListScenes {
  name: string
  properties: Property<unknown>[]
  has_preview?: boolean
  needs_desktop?: boolean
  category: string
  capabilities?: SceneCapabilities
  preview?: ScenePreviewSpec
}

export interface Property<T = unknown> {
  default_value: T
  name: string
  additional?: PropertyUiMetadata
  type_id: TypeId
}

export type CollectionProvider = {
  type: "collection";
  uuid: string;
  arguments: { images: string[] };
};

export type PagesProvider = {
  type: "pages";
  uuid: string;
  arguments: { begin: number; end: number };
};

export type RandomShaderProvider = {
  type: "random";
  uuid: string;
  arguments: { min_page: number; max_page: number };
};

export type CollectionShaderProvider = {
  type: "shader_collection";
  uuid: string;
  arguments: { urls: string[] };
};

export type ListProviders = ListScenes;

export type ProviderValue =
  | CollectionProvider
  | PagesProvider
  | RandomShaderProvider
  | CollectionShaderProvider
  | { type: string; uuid: string; arguments: Record<string, unknown> };

export type TypeId =
  | "string" | "int" | "double" | "bool" | "float" | "millis"
  | "json" | "int16_t" | "uint8_t" | "enum" | "color" | "string[]";
