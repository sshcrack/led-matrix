import { createContext, useContext } from 'react'

/**
 * The daemon serves this UI, so its API is the page's own origin. `pnpm dev` proxies `/__api`
 * to the daemon (LED_MATRIX_DEV_API, default http://localhost:8080; see vite.config.ts).
 */
export const defaultApiUrl = () => window.location.origin + (import.meta.env.DEV ? '/__api' : '')

export const ApiUrlContext = createContext<string | null>(defaultApiUrl())

/** Kept nullable for the plugin SDK's sake (plugins check it); it is always set now. */
export function useApiUrl(): string | null {
  return useContext(ApiUrlContext)
}
