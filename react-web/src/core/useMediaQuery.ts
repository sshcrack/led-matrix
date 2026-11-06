import { useSyncExternalStore } from 'react'

/** Subscribes to a CSS media query (e.g. `(min-width: 1280px)`); false during SSR/tests without matchMedia. */
export function useMediaQuery(query: string): boolean {
  return useSyncExternalStore(
    notify => {
      if (typeof window.matchMedia !== 'function') return () => {}
      const mq = window.matchMedia(query)
      mq.addEventListener('change', notify)
      return () => mq.removeEventListener('change', notify)
    },
    () => typeof window.matchMedia === 'function' && window.matchMedia(query).matches,
  )
}
