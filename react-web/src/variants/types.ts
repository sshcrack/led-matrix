import type { ComponentType, ReactNode } from 'react'
import type { VariantId } from '~/core/variants'

/**
 * A UI variant = a shell (navigation/chrome), a home screen and a preset editor.
 * Everything else (data, actions, autosave, plugin extensions, the other pages)
 * is shared from ~/core, so a variant is only layout + look + interaction design.
 * Adding or removing a variant = one folder + one line in variants/index.ts.
 */
export interface VariantModule {
  id: VariantId
  name: string
  tagline: string
  Shell: ComponentType<{ children: ReactNode }>
  Home: ComponentType
  /** Routed at /modify-preset/:preset_id */
  Editor: ComponentType
  /** Extra <Route> elements this variant needs (e.g. a presets tab). */
  routes?: ReactNode
}
