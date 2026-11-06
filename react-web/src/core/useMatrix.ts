import { useCallback, useEffect, useMemo, useRef } from 'react'
import { toast } from 'sonner'
import { v4 as uuidv4 } from 'uuid'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import type { Status } from '~/apiTypes/status'
import type { ListPresets, RawPreset, Scene } from '~/apiTypes/list_presets'
import type { ListScenes } from '~/apiTypes/list_scenes'
import { useResource } from './useResource'
import { beginPresetDelete, enqueueOp, enqueueSave, finishPresetDelete, isPresetLive, reconcilePresetQueue } from './presetSaveQueue'

export interface Pack { id: string; name: string; description: string; colors: string[]; available: boolean; missing_scenes: string[] }

/**
 * Single data + action layer shared by every UI variant. Variants only decide
 * how things look and how many taps they need; they never talk to the API
 * directly, so behaviour (optimistic updates, toasts, error handling) is
 * identical whichever variant is active.
 */
export function useMatrixState() {
  const apiUrl = useApiUrl()
  const statusR = useResource<Status>('/status', { pollMs: 6000 })
  const presetsR = useResource<ListPresets>('/list_presets')
  const scenesR = useResource<ListScenes[]>('/list_scenes')
  const { setData: setStatus, refresh: refreshStatus } = statusR
  const { setData: setPresets, refresh: refreshPresets } = presetsR
  const presetsRef = useRef(presetsR.data)
  useEffect(() => { presetsRef.current = presetsR.data })
  useEffect(() => { if (presetsR.data) reconcilePresetQueue(Object.keys(presetsR.data)) }, [presetsR.data])

  const call = useCallback(async (path: string, init?: RequestInit) => {
    if (!apiUrl) throw new Error('No API')
    const res = await fetch(apiUrl + path, init)
    if (!res.ok) throw new Error(await res.text().catch(() => '') || res.statusText)
    return res
  }, [apiUrl])

  const json = (body: unknown): RequestInit => ({ method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })

  const activate = useCallback(async (id: string) => {
    setStatus(s => s && { ...s, current: id, operation_mode: 'manual', automatic_active: false })
    try { await call(`/set_active?id=${encodeURIComponent(id)}`) } catch { toast.error('Could not activate preset'); void refreshStatus() }
  }, [call, setStatus, refreshStatus])

  const actions = useMemo(() => ({
    activate,
    async setPower(on: boolean) {
      setStatus(s => s && { ...s, turned_off: !on })
      try { await call(`/set_enabled?enabled=${on}`) } catch { toast.error('Could not change power'); void refreshStatus() }
    },
    async setMode(mode: 'automatic' | 'manual') {
      setStatus(s => s && { ...s, operation_mode: mode, automatic_active: mode === 'automatic' })
      try { await call(`/operation_mode?mode=${mode}`) } catch { toast.error('Could not change mode'); void refreshStatus() }
    },
    /** Create a preset from scenes and (optionally) start playing it – one call chain, one tap. */
    async createPreset(input: { name: string; scenes: Scene[]; play?: boolean; transition_name?: string; transition_duration?: number }): Promise<string | null> {
      try {
        const res = await call('/add_preset', json({
          scenes: input.scenes, display_name: input.name,
          transition_name: input.transition_name ?? 'blend', transition_duration: input.transition_duration ?? 750,
        }))
        const { id } = await res.json() as { id: string }
        await refreshPresets()
        if (input.play) await activate(id)
        toast.success(input.play ? `Playing “${input.name}”` : `Created “${input.name}”`)
        return id
      } catch { toast.error('Could not create preset'); return null }
    },
    async duplicatePreset(id: string, cached: RawPreset) {
      // The list may be stale (edited elsewhere); copy the daemon's current version when reachable.
      const preset = await call(`/presets?id=${encodeURIComponent(id)}`).then(r => r.json() as Promise<RawPreset>).catch(() => cached)
      const copy: RawPreset = {
        ...preset, display_name: `${preset.display_name ?? 'Preset'} copy`,
        scenes: preset.scenes.map(s => ({ ...s, uuid: uuidv4() })),
      }
      try { await call('/add_preset', json(copy)); await refreshPresets(); toast.success('Duplicated') } catch { toast.error('Could not duplicate') }
      return id
    },
    async renamePreset(id: string, name: string) {
      setPresets(p => p && p[id] ? { ...p, [id]: { ...p[id], display_name: name } } : p)
      // Queued behind this preset's autosaves so an older draft can't write the previous name back afterwards.
      try { await enqueueOp(id, () => call(`/preset_display_name?id=${encodeURIComponent(id)}`, json({ display_name: name })).then(() => undefined)) }
      catch { toast.error('Could not rename'); void refreshPresets() }
    },
    async deletePreset(id: string, name: string) {
      await beginPresetDelete(id)   // drop queued autosaves and wait for the one in flight
      try {
        await call(`/preset?id=${encodeURIComponent(id)}`, { method: 'DELETE' })
        finishPresetDelete(id, true)
        await refreshPresets(); toast.success(`Deleted “${name}”`)
      } catch {
        finishPresetDelete(id, false)
        toast.error('Could not delete (the active preset can’t be deleted)')
      }
    },
    /** Saves are queued per preset (see presetSaveQueue.ts). `keepalive` lets the request finish while the page unloads (~64 KB cap). */
    savePreset(id: string, preset: RawPreset, opts?: { keepalive?: boolean }) {
      return enqueueSave(id, preset, async draft => {
        const nameAtStart = presetsRef.current?.[id]?.display_name
        await call(`/preset?id=${encodeURIComponent(id)}`, { ...json(draft), keepalive: opts?.keepalive })
        if (!isPresetLive(id)) return
        // A rename made while this save was in flight is newer than the name in the draft; keep it.
        setPresets(p => {
          if (!p) return p
          const now = p[id]?.display_name
          const name = now !== undefined && now !== nameAtStart ? now : draft.display_name
          return { ...p, [id]: { ...draft, display_name: name } }
        })
      })
    },
    async addPack(packId: string, play: boolean) {
      try {
        const res = await call(`/ambiance_packs?id=${encodeURIComponent(packId)}`, { method: 'POST' })
        const { id, display_name } = await res.json() as { id: string; display_name: string }
        await refreshPresets()
        if (play) await activate(id)
        toast.success(play ? `Playing “${display_name}”` : `Added “${display_name}”`)
      } catch { toast.error('Could not add mood') }
    },
  }), [activate, call, setStatus, setPresets, refreshStatus, refreshPresets])

  const status = statusR.data
  const presets = presetsR.data
  const scenes = scenesR.data
  const current = status && presets ? presets[status.current] : undefined
  const sceneByName = useMemo(() => new Map((scenes ?? []).map(s => [s.name, s])), [scenes])

  return {
    apiUrl, status, presets, scenes, sceneByName, current,
    online: !statusR.error, loading: statusR.loading || presetsR.loading || scenesR.loading,
    error: statusR.error ?? presetsR.error ?? scenesR.error,
    refreshAll: () => { void refreshStatus(); void refreshPresets(); void scenesR.refresh() },
    ...actions,
  }
}
export type Matrix = ReturnType<typeof useMatrixState>
