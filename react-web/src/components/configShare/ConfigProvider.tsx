import React, { createContext, useContext, useState, useCallback } from 'react'
import { toast } from 'sonner'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import type { Preset, RawPreset } from '~/apiTypes/list_presets'
import { objectToArrayPresets } from '~/apiTypes/list_presets'

interface ConfigContextType {
  preset: Preset | null
  setPreset: React.Dispatch<React.SetStateAction<Preset | null>>
  isDirty: boolean
  setDirty: (dirty: boolean) => void
  save: () => Promise<void>
  isSaving: boolean
}

const ConfigContext = createContext<ConfigContextType>({
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

interface ConfigProviderProps {
  children: React.ReactNode
  presetId: string
}

export function ConfigProvider({ children, presetId }: ConfigProviderProps) {
  const apiUrl = useApiUrl()
  const [preset, setPreset] = useState<Preset | null>(null)
  const [isDirty, setDirty] = useState(false)
  const [isSaving, setSaving] = useState(false)

  const wrappedSetPreset: React.Dispatch<React.SetStateAction<Preset | null>> = useCallback((value) => {
    setPreset(value)
    setDirty(true)
  }, [])

  const save = useCallback(async () => {
    if (!apiUrl || !preset) return
    setSaving(true)
    try {
      const rawPreset: RawPreset = objectToArrayPresets(preset)
      const res = await fetch(`${apiUrl}/preset?id=${encodeURIComponent(presetId)}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(rawPreset),
      })
      if (!res.ok) throw new Error('Failed to save')
      setDirty(false)
      toast.success('Preset saved')
    } catch {
      toast.error('Failed to save preset')
    } finally {
      setSaving(false)
    }
  }, [apiUrl, preset, presetId])

  return (
    <ConfigContext.Provider value={{
      preset,
      setPreset: wrappedSetPreset,
      isDirty,
      setDirty,
      save,
      isSaving,
    }}>
      {children}
    </ConfigContext.Provider>
  )
}
