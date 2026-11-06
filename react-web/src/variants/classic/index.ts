import { lazy } from 'react'
import Layout from '~/components/Layout'
import type { VariantModule } from '../types'

/** The UI as it was before the overhaul – kept only for side-by-side comparison. */
const classic: VariantModule = {
  id: 'classic', name: 'Current (before)', tagline: 'The existing UI, unchanged',
  Shell: Layout, Home: lazy(() => import('~/pages/Home')), Editor: lazy(() => import('~/pages/ModifyPreset')),
}
export default classic
