import React, { createContext, useContext } from 'react'
import type { Preset } from '~/apiTypes/list_presets'

export interface ConfigContextType {
  preset: Preset | null
  setPreset: React.Dispatch<React.SetStateAction<Preset | null>>
  isDirty: boolean
  setDirty: (dirty: boolean) => void
  save: () => Promise<void>
  isSaving: boolean
}

export const ConfigContext = createContext<ConfigContextType>({
  preset: null,
  setPreset: () => {},
  isDirty: false,
  setDirty: () => {},
  save: async () => {},
  isSaving: false,
})

export function useConfig() {
  return useContext(ConfigContext)
}
