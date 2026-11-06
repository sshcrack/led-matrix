import { createElement, Fragment, lazy } from 'react'
import { Route } from 'react-router-dom'
import type { VariantModule } from '../types'
import DeckShell from './Shell'

const Home = lazy(() => import('./Home'))
const Editor = lazy(() => import('./Editor'))
const Library = lazy(() => import('./Library'))
const More = lazy(() => import('./More'))

const deck: VariantModule = {
  id: 'deck', name: 'Deck', tagline: 'Phone-first remote · big tap targets · sheets',
  Shell: DeckShell, Home, Editor,
  routes: createElement(Fragment, null,
    createElement(Route, { path: '/presets', element: createElement(Library) }),
    createElement(Route, { path: '/more', element: createElement(More) })),
}
export default deck
