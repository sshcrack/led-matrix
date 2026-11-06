import type { RawPreset } from '~/apiTypes/list_presets'

/**
 * One save queue per preset id, shared by every editor instance and by deletion.
 *
 *  - Saves for a preset run strictly one at a time, in the order they were queued,
 *    so a slow response of an old editor can never land after a newer editor's save.
 *  - While a save is running only the *latest* queued draft is kept (older queued
 *    drafts are superseded; their callers resolve when the newer one is persisted).
 *  - The queue outlives the editor component: closing the editor does not drop a
 *    pending save.
 *  - Deleting a preset first drops its queued draft and waits for the running save,
 *    then marks it deleted so late saves can neither re-create it nor re-insert it
 *    into the list. A preset that shows up in the list again (restored through
 *    /add_preset?id=…) is un-marked by `reconcilePresetQueue`.
 */
type State = 'live' | 'deleting' | 'gone'
interface Job { draft: RawPreset; send: (draft: RawPreset) => Promise<void>; waiters: { resolve: () => void; reject: (e: unknown) => void }[] }
interface Entry { state: State; running: Promise<void> | null; pending: Job | null }

const entries = new Map<string, Entry>()
const entry = (id: string): Entry => {
  let e = entries.get(id)
  if (!e) entries.set(id, e = { state: 'live', running: null, pending: null })
  return e
}

export const isPresetBlocked = (id: string) => (entries.get(id)?.state ?? 'live') !== 'live'
export const isPresetLive = (id: string) => !isPresetBlocked(id)

async function pump(e: Entry) {
  while (e.pending) {
    const job = e.pending
    e.pending = null
    try {
      if (e.state === 'live') await job.send(job.draft)
      job.waiters.forEach(w => w.resolve())
    } catch (err) { job.waiters.forEach(w => w.reject(err)) }
  }
  e.running = null
}

/** Resolves once `draft` (or a newer draft that superseded it) has been sent; rejects if that send failed. */
export function enqueueSave(id: string, draft: RawPreset, send: Job['send']): Promise<void> {
  const e = entry(id)
  return new Promise<void>((resolve, reject) => {
    const waiters = [...(e.pending?.waiters ?? []), { resolve, reject }]
    e.pending = { draft, send, waiters }
    if (!e.running) e.running = pump(e)
  })
}

/** Stop saving this preset and wait for the save in flight, so a following DELETE is the last write. */
export async function beginPresetDelete(id: string) {
  const e = entry(id)
  e.state = 'deleting'
  e.pending?.waiters.forEach(w => w.resolve())
  e.pending = null
  await e.running
}
export const finishPresetDelete = (id: string, ok: boolean) => { const e = entry(id); e.state = ok ? 'gone' : 'live' }

/** Call with the ids of the current preset list: a deleted preset that reappears is live again. */
export function reconcilePresetQueue(ids: Iterable<string>) {
  for (const id of ids) { const e = entries.get(id); if (e?.state === 'gone') e.state = 'live' }
}
