import type { VariantId } from '~/core/variants'
import type { VariantModule } from './types'
import consoleVariant from './console'
import deck from './deck'
import studio from './studio'
import ambient from './ambient'
import classic from './classic'

/** Registry. To drop a variant after the decision: delete its folder and its line here. */
export const VARIANTS: Partial<Record<VariantId, VariantModule>> = {
  console: consoleVariant,
  deck,
  studio,
  ambient,
  classic,
}
export const variantList = () => Object.values(VARIANTS) as VariantModule[]
export const getVariant = (id: VariantId): VariantModule => VARIANTS[id] ?? consoleVariant
