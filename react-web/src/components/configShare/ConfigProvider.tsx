import React, { useState, useCallback } from 'react'
import { toast } from 'sonner'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { useDraftSave } from '~/lib/draftSave'
import { ConfigContext } from './configContext'
import type { Preset, RawPreset } from '~/apiTypes/list_presets'
import { objectToArrayPresets } from '~/apiTypes/list_presets'

interface ConfigProviderProps {
  children: React.ReactNode
  presetId: string
}

export function ConfigProvider({ children, presetId }: ConfigProviderProps) {
  const apiUrl = useApiUrl()
  const [preset, setPreset] = useState<Preset | null>(null)
  const draft = useDraftSave()
  const { edited, loaded } = draft

  const wrappedSetPreset: React.Dispatch<React.SetStateAction<Preset | null>> = useCallback((value) => {
    setPreset(value)
    edited()
  }, [edited])

  const loadPreset = useCallback((value: Preset) => {
    setPreset(value)
    loaded()
  }, [loaded])

  const { save: saveDraft } = draft
  const save = useCallback(async () => {
    if (!apiUrl || !preset) return
    try {
      const saved = await saveDraft(async () => {
        const rawPreset: RawPreset = objectToArrayPresets(preset)
        const res = await fetch(`${apiUrl}/preset?id=${encodeURIComponent(presetId)}`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(rawPreset),
        })
        if (!res.ok) throw new Error('Failed to save')
      })
      if (saved) toast.success('Preset saved')
    } catch {
      toast.error('Failed to save preset')
    }
  }, [apiUrl, preset, presetId, saveDraft])

  return (
    <ConfigContext.Provider value={{
      preset,
      setPreset: wrappedSetPreset,
      loadPreset,
      isDirty: draft.isDirty,
      save,
      isSaving: draft.isSaving,
    }}>
      {children}
    </ConfigContext.Provider>
  )
}
