import { useCallback, useEffect, useRef, useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import type { RawPreset, Scene } from '~/apiTypes/list_presets'
import { useMatrix } from './matrixContext'

export type SaveState = 'saved' | 'dirty' | 'saving' | 'error'

/**
 * Edits one preset with autosave: every change is persisted ~0.7 s after the
 * last edit (and on unmount / tab hide), so there is no Save button to forget.
 * Revision tracking means an edit made while a save is in flight stays dirty.
 */
export function usePresetEditor(presetId: string, debounceMs = 700) {
  const apiUrl = useApiUrl()
  const { savePreset, presets } = useMatrix()
  const sharedName = presets?.[presetId]?.display_name
  // True once the user typed a name here; until then the list's name (e.g. from a menu rename) wins.
  const nameEdited = useRef(false)
  const [preset, setPreset] = useState<RawPreset | null>(null)
  const [loadError, setLoadError] = useState(false)
  const [saveState, setSaveState] = useState<SaveState>('saved')
  const latest = useRef<RawPreset | null>(null)
  const revision = useRef(0)
  const savedRevision = useRef(0)
  const inFlight = useRef(false)
  const timer = useRef<number | null>(null)
  const activeId = useRef(presetId)
  const sharedNameRef = useRef(sharedName)
  useEffect(() => { sharedNameRef.current = sharedName }, [sharedName])
  // Show a rename made elsewhere (rail menu) instead of the stale loaded name.
  useEffect(() => {
    if (nameEdited.current || sharedName === undefined || !latest.current || latest.current.display_name === sharedName) return
    latest.current = { ...latest.current, display_name: sharedName }
    setPreset(latest.current)
  }, [sharedName])

  useEffect(() => {
    if (!apiUrl || !presetId) return
    const c = new AbortController()
    // New preset: drop the previous preset's draft/revisions so nothing of it can be saved under this id.
    activeId.current = presetId
    latest.current = null; revision.current = savedRevision.current = 0; nameEdited.current = false
    if (timer.current) { window.clearTimeout(timer.current); timer.current = null }
    fetch(`${apiUrl}/presets?id=${encodeURIComponent(presetId)}`, { signal: c.signal })
      .then(r => { if (!r.ok) throw new Error(); return r.json() as Promise<RawPreset> })
      .then(p => { latest.current = p; revision.current = savedRevision.current = 0; setPreset(p); setLoadError(false); setSaveState('saved') })
      .catch(e => { if (e?.name !== 'AbortError') setLoadError(true) })
    return () => c.abort()
  }, [apiUrl, presetId])

  const flush = useCallback(async () => {
    if (timer.current) { window.clearTimeout(timer.current); timer.current = null }
    if (inFlight.current || !latest.current || revision.current === savedRevision.current) return
    const rev = revision.current
    const draft = !nameEdited.current && sharedNameRef.current !== undefined
      ? { ...latest.current, display_name: sharedNameRef.current } : latest.current
    inFlight.current = true
    setSaveState('saving')
    try {
      await savePreset(presetId, draft)
      if (activeId.current !== presetId) return   // switched away mid-save: this editor state now belongs to another preset
      savedRevision.current = rev
      if (revision.current === rev) nameEdited.current = false
      setSaveState(revision.current === rev ? 'saved' : 'dirty')
    } catch { if (activeId.current === presetId) setSaveState('error') }
    finally {
      inFlight.current = false
      if (activeId.current === presetId && revision.current !== savedRevision.current && !timer.current) timer.current = window.setTimeout(() => void flush(), debounceMs)
    }
  }, [presetId, savePreset, debounceMs])

  const edit = useCallback((fn: (p: RawPreset) => RawPreset) => {
    if (!latest.current) return
    latest.current = fn(latest.current)
    revision.current++
    setPreset(latest.current)
    setSaveState('dirty')
    if (timer.current) window.clearTimeout(timer.current)
    timer.current = window.setTimeout(() => void flush(), debounceMs)
  }, [flush, debounceMs])

  useEffect(() => {
    const onHide = () => { if (document.visibilityState === 'hidden') void flush() }
    document.addEventListener('visibilitychange', onHide)
    return () => { document.removeEventListener('visibilitychange', onHide); void flush() }
  }, [flush])

  return {
    preset, loadError, saveState, retry: flush,
    updateScene: (scene: Scene) => edit(p => ({ ...p, scenes: p.scenes.map(s => s.uuid === scene.uuid ? scene : s) })),
    addScenes: (scenes: Scene[]) => edit(p => ({ ...p, scenes: [...p.scenes, ...scenes] })),
    restoreScene: (scene: Scene, index: number) => edit(p => { const scenes = [...p.scenes]; scenes.splice(Math.min(index, scenes.length), 0, scene); return { ...p, scenes } }),
    removeScene: (uuid: string) => edit(p => ({ ...p, scenes: p.scenes.filter(s => s.uuid !== uuid) })),
    moveScene: (uuid: string, delta: -1 | 1) => edit(p => {
      const i = p.scenes.findIndex(s => s.uuid === uuid), j = i + delta
      if (i < 0 || j < 0 || j >= p.scenes.length) return p
      const scenes = [...p.scenes];[scenes[i], scenes[j]] = [scenes[j], scenes[i]]
      return { ...p, scenes }
    }),
    setTransition: (patch: { transition_name?: string; transition_duration?: number }) => edit(p => ({ ...p, ...patch })),
    setName: (display_name: string) => { nameEdited.current = true; edit(p => ({ ...p, display_name })) },
  }
}
export type PresetEditorApi = ReturnType<typeof usePresetEditor>
