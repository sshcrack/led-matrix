import type { ListPresets, RawPreset } from '~/apiTypes/list_presets'

/**
 * Preset persistence: the one module every preset writer and reader in this browser goes through
 * (inspector autosave, menu rename/delete, source editors, scene gallery).
 *
 *  - Writes for a preset run strictly one at a time, in the order they were queued, so a slow
 *    response of an old editor can never land after a newer write.
 *  - Consecutive queued *saves* coalesce: only the latest full-preset draft is kept (older ones are
 *    superseded; their callers resolve when the newer one is persisted). Renames and updates are
 *    ordered barriers between saves.
 *  - `load` waits until nothing is queued for the preset, so a reader never starts from state older
 *    than what is still being written (e.g. a closed editor's last autosave).
 *  - `remove` waits for the write in flight and holds further writes back. If the DELETE succeeds
 *    the held writes are discarded; if it fails they resume (nothing is lost). A removed preset that
 *    shows up in the list again (restored through /add_preset?id=…) is writable again after `reconcile`.
 *  - Acknowledged writes are published to the shared preset list, keeping a rename that happened
 *    while the write was in flight.
 *
 * The queue lives as long as this object (one per MatrixProvider), not as long as an editor.
 * Cross-browser conflicts are out of scope: the daemon has no revisions to detect them.
 */
export interface PresetTransport {
  get(id: string, signal?: AbortSignal): Promise<RawPreset>
  put(id: string, preset: RawPreset, opts?: { keepalive?: boolean }): Promise<void>
  rename(id: string, name: string): Promise<void>
  remove(id: string): Promise<void>
}

/** Updates the shared preset list the UI renders (useMatrix's `presets`); the list comes back through `listed`. */
export type PresetListWriter = (update: (list: ListPresets | null) => ListPresets | null) => void

export interface PresetPersistence {
  /** GET the preset once all of its queued writes have landed. */
  load(id: string, signal?: AbortSignal): Promise<RawPreset>
  /** Persist a full draft. Resolves once it (or a newer draft that superseded it) was sent; rejects if that send failed. */
  save(id: string, draft: RawPreset, opts?: { keepalive?: boolean }): Promise<void>
  /** Read-modify-write against the daemon's current version, ordered with every other write. */
  update(id: string, change: (preset: RawPreset) => RawPreset): Promise<RawPreset>
  /** Show the new name immediately and write it after everything already queued. */
  rename(id: string, name: string): Promise<void>
  /** Delete; queued writes are dropped on success and resume on failure (which rethrows). */
  remove(id: string): Promise<void>
  /** False while the preset is being (or has been) removed: editors should not queue drafts. */
  canWrite(id: string): boolean
  /** Call with every list the UI renders. A removed preset that reappears is writable again. */
  listed(list: ListPresets): void
}

type State = 'live' | 'deleting' | 'gone'
interface Waiter { resolve: () => void; reject: (e: unknown) => void }
type Job =
  | { kind: 'save'; draft: RawPreset; keepalive: boolean; waiters: Waiter[] }
  | { kind: 'op'; run: () => Promise<void>; waiters: Waiter[] }
interface Entry { state: State; running: Promise<void> | null; queue: Job[] }

export function createPresetPersistence(transport: PresetTransport, writeList: PresetListWriter): PresetPersistence {
  let known: ListPresets | null = null
  const entries = new Map<string, Entry>()
  const entry = (id: string): Entry => {
    let e = entries.get(id)
    if (!e) entries.set(id, e = { state: 'live', running: null, queue: [] })
    return e
  }
  const canWrite = (id: string) => (entries.get(id)?.state ?? 'live') === 'live'

  /** Publish an acknowledged preset, keeping a list name that changed while the write was in flight. */
  const publish = (id: string, preset: RawPreset, nameAtStart: string | undefined) => {
    if (!canWrite(id)) return
    writeList(list => {
      if (!list) return list
      const now = list[id]?.display_name
      const display_name = now !== undefined && now !== nameAtStart ? now : preset.display_name
      return { ...list, [id]: { ...preset, display_name } }
    })
  }

  const send = async (id: string, draft: RawPreset, keepalive: boolean) => {
    const nameAtStart = known?.[id]?.display_name
    await transport.put(id, draft, keepalive ? { keepalive } : undefined)
    publish(id, draft, nameAtStart)
  }

  async function pump(id: string, e: Entry) {
    while (e.state === 'live' && e.queue.length) {
      const job = e.queue.shift()!
      try {
        if (job.kind === 'save') await send(id, job.draft, job.keepalive)
        else await job.run()
        job.waiters.forEach(w => w.resolve())
      } catch (err) { job.waiters.forEach(w => w.reject(err)) }
    }
  }
  const kick = (id: string, e: Entry) => {
    if (e.running || e.state !== 'live') return
    const run: Promise<void> = pump(id, e).finally(() => {
      if (e.running === run) e.running = null
      // A waiter continuation may have queued more work while `running` was still set.
      if (e.queue.length) kick(id, e)
    })
    e.running = run
  }
  const enqueueOp = (id: string, run: () => Promise<void>) => {
    const e = entry(id)
    return new Promise<void>((resolve, reject) => { e.queue.push({ kind: 'op', run, waiters: [{ resolve, reject }] }); kick(id, e) })
  }
  const whenIdle = async (id: string) => {
    const e = entries.get(id)
    // Loop: new writes may arrive while waiting. Live + queued but not running cannot happen (kick).
    while (e?.running) await e.running
  }

  return {
    async load(id, signal) {
      await whenIdle(id)
      signal?.throwIfAborted()
      return transport.get(id, signal)
    },

    save(id, draft, opts) {
      const e = entry(id)
      return new Promise<void>((resolve, reject) => {
        const last = e.queue.at(-1)
        const waiters = [...(last?.kind === 'save' ? last.waiters : []), { resolve, reject }]
        const job: Job = { kind: 'save', draft, keepalive: !!opts?.keepalive, waiters }
        if (last?.kind === 'save') e.queue[e.queue.length - 1] = job
        else e.queue.push(job)
        kick(id, e)
      })
    },

    async update(id, change) {
      let result: RawPreset | null = null
      await enqueueOp(id, async () => {
        const next = change(await transport.get(id))
        await send(id, next, false)
        result = next
      })
      return result!
    },

    async rename(id, name) {
      writeList(list => list && list[id] ? { ...list, [id]: { ...list[id], display_name: name } } : list)
      // Queued behind this preset's autosaves so an older draft can't write the previous name back afterwards.
      await enqueueOp(id, () => transport.rename(id, name))
    },

    async remove(id) {
      const e = entry(id)
      e.state = 'deleting'
      await e.running   // the write in flight finishes; later ones are held back
      try {
        await transport.remove(id)
      } catch (err) {
        e.state = 'live'; kick(id, e)   // resume the writes that were held back
        throw err
      }
      e.state = 'gone'
      e.queue.splice(0).forEach(j => j.waiters.forEach(w => w.resolve()))   // the preset no longer exists
    },

    canWrite,

    listed(list) {
      known = list
      for (const id of Object.keys(list)) { const e = entries.get(id); if (e?.state === 'gone') { e.state = 'live'; kick(id, e) } }
    },
  }
}

/** Production transport: the daemon's REST routes. */
export function httpPresetTransport(apiUrl: string): PresetTransport {
  const q = (id: string) => `id=${encodeURIComponent(id)}`
  const check = async (res: Response) => {
    if (!res.ok) throw new Error(await res.text().catch(() => '') || res.statusText)
    return res
  }
  const post = (path: string, body: unknown, keepalive?: boolean) => fetch(apiUrl + path, {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body), keepalive,
  }).then(check)
  return {
    get: (id, signal) => fetch(`${apiUrl}/presets?${q(id)}`, { signal }).then(check).then(r => r.json() as Promise<RawPreset>),
    put: async (id, preset, opts) => { await post(`/preset?${q(id)}`, preset, opts?.keepalive) },
    rename: async (id, name) => { await post(`/preset_display_name?${q(id)}`, { display_name: name }) },
    remove: async id => { await fetch(`${apiUrl}/preset?${q(id)}`, { method: 'DELETE' }).then(check) },
  }
}
