import React, { useState } from 'react'
import type { ProviderValue } from '~/apiTypes/list_scenes'
import { ProviderDataContext } from './providerDataContext'

export function ProviderDataProvider({ children, initial }: { children: React.ReactNode; initial: ProviderValue[] }) {
  const [providers, setProviders] = useState<ProviderValue[]>(initial)
  return (
    <ProviderDataContext.Provider value={{ providers, setProviders }}>
      {children}
    </ProviderDataContext.Provider>
  )
}
