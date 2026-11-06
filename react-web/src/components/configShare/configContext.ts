import React, { createContext, useContext } from 'react'
import type { Preset } from '~/apiTypes/list_presets'

export interface ConfigContextType {
  preset: Preset | null
  setPreset: React.Dispatch<React.SetStateAction<Preset | null>>
  /** Replaces the draft with server data without marking it dirty. */
  loadPreset: (preset: Preset) => void
  isDirty: boolean
  save: () => Promise<void>
  isSaving: boolean
}

export const ConfigContext = createContext<ConfigContextType>({
  preset: null,
  setPreset: () => {},
  loadPreset: () => {},
  isDirty: false,
  save: async () => {},
  isSaving: false,
})

export function useConfig() {
  return useContext(ConfigContext)
}
