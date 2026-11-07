import type { ListPresets, RawPreset } from '~/apiTypes/list_presets'
import type { PresetListWriter, PresetTransport } from './presetPersistence'

/**
 * In-memory daemon + preset list for tests of preset persistence and its callers. Like useMatrix,
 * every list change is shown back to the persistence (`attach`), as a render would.
 * `hold()` makes the next write wait until `release()` is called, to test writes in flight.
 */
export function createTestPresetDaemon(initial: ListPresets) {
  const daemon: ListPresets = structuredClone(initial)
  let list: ListPresets | null = structuredClone(initial)
  const log: string[] = []
  const puts: { id: string; preset: RawPreset }[] = []
  let held = 0
  const releases: (() => void)[] = []
  let failNext: string | null = null
  const gate = async (what: string) => {
    if (failNext === what) { failNext = null; throw new Error(`${what} failed`) }
    if (held > 0) { held--; await new Promise<void>(r => releases.push(r)) }
  }

  const transport: PresetTransport & { gets: number } = {
    gets: 0,
    async get(id) {
      transport.gets++
      if (!daemon[id]) throw new Error('not found')
      return structuredClone(daemon[id])
    },
    async put(id, preset) {
      log.push(`put ${id} ${preset.display_name}`)
      await gate('put')
      puts.push({ id, preset })
      daemon[id] = structuredClone(preset)
    },
    async rename(id, name) {
      log.push(`rename ${id} ${name}`)
      await gate('rename')
      daemon[id] = { ...daemon[id], display_name: name }
    },
    async remove(id) {
      log.push(`remove ${id}`)
      await gate('remove')
      delete daemon[id]
      writeList(l => { if (!l) return l; const rest = { ...l }; delete rest[id]; return rest })   // the list refresh after a delete
    },
  }
  let observer: ((list: ListPresets) => void) | null = null
  const show = () => { if (list) observer?.(list) }
  const writeList: PresetListWriter = update => { list = update(list); show() }

  return {
    transport, writeList, daemon, log, puts,
    attach(listed: (list: ListPresets) => void) { observer = listed; show() },
    get list() { return list },
    hold: (n = 1) => { held += n },
    release: () => releases.shift()?.(),
    failNext: (what: 'put' | 'rename' | 'remove') => { failNext = what },
  }
}

export const mkPreset = (name: string): RawPreset => ({
  display_name: name, transition_name: 'blend', transition_duration: 0,
  scenes: [{ uuid: 'u', type: 'clock', arguments: {} }],
}) as RawPreset
