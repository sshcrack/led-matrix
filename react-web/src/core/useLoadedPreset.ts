import { useEffect, useState } from 'react'
import type { RawPreset } from '~/apiTypes/list_presets'
import { useMatrix } from './matrixContext'

/** The daemon's current version of a preset, loaded only after this browser's queued writes for it landed. */
export function useLoadedPreset(presetId: string) {
  const { persistence } = useMatrix()
  const [loaded, setLoaded] = useState<{ id: string; preset: RawPreset | null; error: boolean } | null>(null)
  useEffect(() => {
    if (!presetId) return
    const c = new AbortController()
    persistence.load(presetId, c.signal)
      .then(preset => setLoaded({ id: presetId, preset, error: false }))
      .catch(e => { if (e?.name !== 'AbortError') setLoaded({ id: presetId, preset: null, error: true }) })
    return () => c.abort()
  }, [persistence, presetId])
  const current = loaded?.id === presetId ? loaded : null
  return { data: current?.preset ?? null, isLoading: !current, error: current?.error ?? false }
}
