import { useEffect, useState, type ReactNode } from 'react'
import { useParams } from 'react-router-dom'
import { whenPresetIdle } from './presetSaveQueue'

/**
 * Renders its children once the route's preset has no queued autosaves left, so pages that
 * GET the preset (source editors) never start from a draft older than what the inspector just edited.
 */
export function PresetWritesGate({ children }: { children: ReactNode }) {
  const { preset_id } = useParams<{ preset_id: string }>()
  const id = preset_id ? decodeURIComponent(preset_id) : ''
  const [readyFor, setReadyFor] = useState<string | null>(null)
  useEffect(() => {
    let live = true
    void whenPresetIdle(id).then(() => { if (live) setReadyFor(id) })
    return () => { live = false }
  }, [id])
  return readyFor === id ? <>{children}</> : null
}
