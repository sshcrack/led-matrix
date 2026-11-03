import React, { createContext, useContext } from 'react'
import type { ProviderValue } from '~/apiTypes/list_scenes'

export interface ProviderDataContextType {
  providers: ProviderValue[]
  setProviders: React.Dispatch<React.SetStateAction<ProviderValue[]>>
}

export const ProviderDataContext = createContext<ProviderDataContextType>({
  providers: [],
  setProviders: () => {},
})

export function useProviderData() {
  return useContext(ProviderDataContext)
}
