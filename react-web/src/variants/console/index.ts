import { lazy } from 'react'
import type { VariantModule } from '../types'
import ConsoleShell from './Shell'

const Home = lazy(() => import('./Home'))
const Editor = lazy(() => import('./Editor'))

const consoleVariant: VariantModule = {
  id: 'console', name: 'Console', tagline: 'Everything on one screen · inline editor · ⌘K',
  Shell: ConsoleShell, Home, Editor,
}
export default consoleVariant
