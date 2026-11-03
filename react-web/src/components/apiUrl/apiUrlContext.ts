import { createContext, useContext } from 'react'

export const ApiUrlContext = createContext<string | null>(null)

export function useApiUrl(): string | null {
  return useContext(ApiUrlContext)
}
