import { lazy } from 'react'
import type { VariantModule } from '../types'
import AmbientShell from './Shell'

const ambient: VariantModule = {
  id: 'ambient', name: 'Ambient', tagline: 'The matrix is the hero · moods · almost no chrome',
  Shell: AmbientShell, Home: lazy(() => import('./Home')), Editor: lazy(() => import('./Editor')),
}
export default ambient
