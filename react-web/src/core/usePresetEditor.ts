import { useCallback, useEffect, useRef, useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import type { RawPreset, Scene } from '~/apiTypes/list_presets'
import { useMatrix } from './matrixContext'
import { toast } from 'sonner'
import { isPresetBlocked } from './presetSaveQueue'

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
  // Name edits are tracked apart from scene edits: a name counts as "typed here" only until that exact
  // name revision has been saved, after which a rename from elsewhere may win again.
  const nameRev = useRef(0)
  const savedNameRev = useRef(0)
  const nameEdited = () => nameRev.current > savedNameRev.current
  const [preset, setPreset] = useState<RawPreset | null>(null)
  const [loadError, setLoadError] = useState(false)
  const [saveState, setSaveState] = useState<SaveState>('saved')
  const latest = useRef<RawPreset | null>(null)
  const revision = useRef(0)
  const savedRevision = useRef(0)
  const inFlight = useRef(0)
  const unmounted = useRef(false)
  useEffect(() => { unmounted.current = false; return () => { unmounted.current = true } }, [])
  const timer = useRef<number | null>(null)
  const activeId = useRef(presetId)
  // Name the shared list showed when the preset was fetched, and a rename seen since then.
  const lastShared = useRef(sharedName)
  const renamedTo = useRef<string | undefined>(undefined)
  const sharedNow = useRef(sharedName)
  useEffect(() => { sharedNow.current = sharedName }, [sharedName])
  // Show a rename made elsewhere (rail menu) instead of the stale loaded name.
  useEffect(() => {
    if (sharedName === lastShared.current) return
    lastShared.current = sharedName
    renamedTo.current = sharedName
    if (nameEdited() || sharedName === undefined || !latest.current || latest.current.display_name === sharedName) return
    latest.current = { ...latest.current, display_name: sharedName }
    setPreset(latest.current)
  }, [sharedName])

  useEffect(() => {
    if (!apiUrl || !presetId) return
    const c = new AbortController()
    // New preset: drop the previous preset's draft/revisions so nothing of it can be saved under this id.
    activeId.current = presetId
    latest.current = null; revision.current = savedRevision.current = 0; nameRev.current = savedNameRev.current = 0
    lastShared.current = sharedNow.current; renamedTo.current = undefined
    if (timer.current) { window.clearTimeout(timer.current); timer.current = null }
    fetch(`${apiUrl}/presets?id=${encodeURIComponent(presetId)}`, { signal: c.signal })
      .then(r => { if (!r.ok) throw new Error(); return r.json() as Promise<RawPreset> })
      .then(p => { latest.current = p; revision.current = savedRevision.current = 0; setPreset(p); setLoadError(false); setSaveState('saved') })
      .catch(e => { if (e?.name !== 'AbortError') setLoadError(true) })
    return () => c.abort()
  }, [apiUrl, presetId])

  // `force` (editor closing) sends even while an earlier save is in flight: the shared queue orders them.
  const flush = useCallback(async (keepalive = false, force = false) => {
    if (timer.current) { window.clearTimeout(timer.current); timer.current = null }
    if ((inFlight.current > 0 && !force) || !latest.current || revision.current === savedRevision.current) return
    if (isPresetBlocked(presetId)) { if (timer.current) { window.clearTimeout(timer.current); timer.current = null } return }
    const rev = revision.current
    const nameRevNow = nameRev.current
    const draft = !nameEdited() && renamedTo.current !== undefined
      ? { ...latest.current, display_name: renamedTo.current } : latest.current
    inFlight.current++
    let succeeded = false
    setSaveState('saving')
    try {
      await savePreset(presetId, draft, { keepalive })
      if (activeId.current !== presetId) return   // switched away mid-save: this editor state now belongs to another preset
      savedRevision.current = Math.max(savedRevision.current, rev)
      succeeded = true
      savedNameRev.current = Math.max(savedNameRev.current, nameRevNow)
      setSaveState(revision.current === rev ? 'saved' : 'dirty')
    } catch {
      if (unmounted.current) toast.error(`Could not save your last changes to “${draft.display_name ?? 'preset'}”`)
      else if (activeId.current === presetId) setSaveState('error')
    } finally {
      inFlight.current--
      // Only after a successful save: a failed one (e.g. over the keepalive quota) must not retry in a tight loop.
      if (succeeded && !unmounted.current && activeId.current === presetId && revision.current !== savedRevision.current && !timer.current) {
        // Edits made while saving: if the page is going away don't wait for the debounce.
        if (document.visibilityState === 'hidden') void flush(true)
        else timer.current = window.setTimeout(() => void flush(), debounceMs)
      }
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
    const onHide = () => { if (document.visibilityState === 'hidden') void flush(true) }
    document.addEventListener('visibilitychange', onHide)
    return () => { document.removeEventListener('visibilitychange', onHide); void flush(false, true) }
  }, [flush])

  return {
    preset, loadError, saveState, retry: () => flush(),
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
    setName: (display_name: string) => { nameRev.current++; edit(p => ({ ...p, display_name })) },
  }
}
export type PresetEditorApi = ReturnType<typeof usePresetEditor>
