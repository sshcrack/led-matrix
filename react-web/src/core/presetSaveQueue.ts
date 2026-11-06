import type { RawPreset } from '~/apiTypes/list_presets'

/**
 * One write queue per preset id, shared by every editor instance, menu renames and deletion.
 *
 *  - Writes for a preset run strictly one at a time, in the order they were queued, so a slow
 *    response of an old editor can never land after a newer write.
 *  - Consecutive queued *saves* coalesce: only the latest full-preset draft is kept (older
 *    ones are superseded; their callers resolve when the newer one is persisted). A rename
 *    is an ordered barrier between saves.
 *  - The queue outlives the editor component: closing the editor does not drop a pending
 *    write, and a reopened editor waits for `whenPresetIdle` before loading, so it never
 *    starts from state older than what is still queued.
 *  - Deleting waits for the write in flight and stops further writes. If the DELETE
 *    succeeds the queued writes are discarded; if it fails they resume (nothing is lost).
 *    A deleted preset that shows up in the list again (restored through
 *    /add_preset?id=…) is un-marked by `reconcilePresetQueue`.
 */
type State = 'live' | 'deleting' | 'gone'
interface Waiter { resolve: () => void; reject: (e: unknown) => void }
type Job =
  | { kind: 'save'; draft: RawPreset; run: (draft: RawPreset) => Promise<void>; waiters: Waiter[] }
  | { kind: 'op'; run: () => Promise<void>; waiters: Waiter[] }
interface Entry { state: State; running: Promise<void> | null; queue: Job[] }

const entries = new Map<string, Entry>()
const entry = (id: string): Entry => {
  let e = entries.get(id)
  if (!e) entries.set(id, e = { state: 'live', running: null, queue: [] })
  return e
}

export const isPresetBlocked = (id: string) => (entries.get(id)?.state ?? 'live') !== 'live'
export const isPresetLive = (id: string) => !isPresetBlocked(id)

async function pump(e: Entry) {
  while (e.state === 'live' && e.queue.length) {
    const job = e.queue.shift()!
    try {
      if (job.kind === 'save') await job.run(job.draft)
      else await job.run()
      job.waiters.forEach(w => w.resolve())
    } catch (err) { job.waiters.forEach(w => w.reject(err)) }
  }
}
const kick = (e: Entry) => {
  if (e.running || e.state !== 'live') return
  const run: Promise<void> = pump(e).finally(() => { if (e.running === run) e.running = null })
  e.running = run
}

/** Resolves once `draft` (or a newer draft that superseded it) has been sent; rejects if that send failed. */
export function enqueueSave(id: string, draft: RawPreset, run: (draft: RawPreset) => Promise<void>): Promise<void> {
  const e = entry(id)
  return new Promise<void>((resolve, reject) => {
    const last = e.queue.at(-1)
    const waiters = [...(last?.kind === 'save' ? last.waiters : []), { resolve, reject }]
    const job: Job = { kind: 'save', draft, run, waiters }
    if (last?.kind === 'save') e.queue[e.queue.length - 1] = job
    else e.queue.push(job)
    kick(e)
  })
}

/** Queue any other write (e.g. a rename) behind everything already queued for this preset. */
export function enqueueOp(id: string, run: () => Promise<void>): Promise<void> {
  const e = entry(id)
  return new Promise<void>((resolve, reject) => { e.queue.push({ kind: 'op', run, waiters: [{ resolve, reject }] }); kick(e) })
}

/** Resolves when nothing is running or queued for this preset (loop: new writes may arrive meanwhile). */
export async function whenPresetIdle(id: string) {
  const e = entries.get(id)
  while (e && (e.running || (e.state === 'live' && e.queue.length))) {
    if (e.running) await e.running
    else break   // live + queued but not running cannot happen (kick), don't spin
  }
}

/** Stop writing this preset and wait for the write in flight, so a following DELETE is the last write. */
export async function beginPresetDelete(id: string) {
  const e = entry(id)
  e.state = 'deleting'
  await e.running
}
export function finishPresetDelete(id: string, ok: boolean) {
  const e = entry(id)
  if (ok) {
    e.state = 'gone'
    e.queue.splice(0).forEach(j => j.waiters.forEach(w => w.resolve()))   // the preset no longer exists
  } else { e.state = 'live'; kick(e) }                                      // resume the writes that were held back
}

/** Call with the ids of the current preset list: a deleted preset that reappears is live again. */
export function reconcilePresetQueue(ids: Iterable<string>) {
  for (const id of ids) { const e = entries.get(id); if (e?.state === 'gone') { e.state = 'live'; kick(e) } }
}
