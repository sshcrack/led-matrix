import { useSyncExternalStore } from 'react'

export const VARIANT_IDS = ['console', 'deck', 'studio', 'ambient', 'classic'] as const
export type VariantId = (typeof VARIANT_IDS)[number]
const KEY = 'uiVariant'
const DEFAULT: VariantId = 'console'
/** Variants that ship with a dark-first look unless the user picked a scheme. */
const DARK_FIRST: VariantId[] = ['console', 'ambient']

const isVariant = (v: string | null): v is VariantId => !!v && (VARIANT_IDS as readonly string[]).includes(v)
const listeners = new Set<() => void>()

function read(): VariantId {
  try {
    const fromUrl = new URLSearchParams(window.location.search).get('ui')
    if (isVariant(fromUrl)) { localStorage.setItem(KEY, fromUrl); return fromUrl }
    const stored = localStorage.getItem(KEY)
    if (isVariant(stored)) return stored
  } catch { /* storage unavailable */ }
  return DEFAULT
}

let current: VariantId = read()

/** Applies the variant's design tokens (data-ui) and, if the user never picked a scheme, its default light/dark. */
export function applyVariant(id: VariantId = current) {
  const root = document.documentElement
  root.dataset.ui = id
  let stored: string | null = null
  try { stored = localStorage.getItem('colorScheme') } catch { /* ignore */ }
  const dark = stored ? stored === 'dark' : (DARK_FIRST.includes(id) || window.matchMedia('(prefers-color-scheme: dark)').matches)
  root.classList.toggle('dark', dark)
}

export function setVariant(id: VariantId) {
  current = id
  try { localStorage.setItem(KEY, id) } catch { /* ignore */ }
  const url = new URL(window.location.href)
  if (url.searchParams.has('ui')) { url.searchParams.set('ui', id); window.history.replaceState(null, '', url) }
  applyVariant(id)
  listeners.forEach(l => l())
}

export function useVariantId(): VariantId {
  return useSyncExternalStore(l => { listeners.add(l); return () => { listeners.delete(l) } }, () => current)
}
