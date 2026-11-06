import { lazy } from 'react'
import type { VariantModule } from '../types'
import StudioShell from './Shell'

const studio: VariantModule = {
  id: 'studio', name: 'Studio', tagline: 'Library grid · inline builder · split editor · docked player',
  Shell: StudioShell, Home: lazy(() => import('./Home')), Editor: lazy(() => import('./Editor')),
}
export default studio
