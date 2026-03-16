import React, { createContext, useContext, useState } from 'react'
import type { ProviderValue } from '~/apiTypes/list_scenes'

interface ProviderDataContextType {
  providers: ProviderValue[]
  setProviders: React.Dispatch<React.SetStateAction<ProviderValue[]>>
}

const ProviderDataContext = createContext<ProviderDataContextType>({
  providers: [],
  setProviders: () => {},
})

export function useProviderData() {
  return useContext(ProviderDataContext)
}

export function ProviderDataProvider({ children, initial }: { children: React.ReactNode; initial: ProviderValue[] }) {
  const [providers, setProviders] = useState<ProviderValue[]>(initial)
  return (
    <ProviderDataContext.Provider value={{ providers, setProviders }}>
      {children}
    </ProviderDataContext.Provider>
  )
}
