import { describe, expect, it } from 'vitest'
import { createPresetPersistence } from './presetPersistence'
import { createTestPresetDaemon, mkPreset } from './presetTestTransport'

const setup = () => {
  const d = createTestPresetDaemon({ A: mkPreset('A'), B: mkPreset('B') })
  const p = createPresetPersistence(d.transport, d.writeList)
  d.attach(p.listed)
  return { d, p }
}
const tick = () => new Promise(r => setTimeout(r, 0))

describe('preset persistence', () => {
  it('coalesces queued saves behind the one in flight and resolves every caller', async () => {
    const { d, p } = setup()
    d.hold()
    const first = p.save('A', mkPreset('a1'))
    const second = p.save('A', mkPreset('a2'))
    const third = p.save('A', mkPreset('a3'))
    await tick()
    d.release()
    await Promise.all([first, second, third])
    expect(d.puts.map(x => x.preset.display_name)).toEqual(['a1', 'a3'])
  })

  it('publishes an acknowledged save to the list, keeping a rename made while it was in flight', async () => {
    const { d, p } = setup()
    d.hold()
    const save = p.save('A', { ...mkPreset('A'), scenes: [] })
    await tick()
    const rename = p.rename('A', 'Menu name')                     // optimistic: list shows it at once
    expect(d.list?.A.display_name).toBe('Menu name')
    d.release()
    await Promise.all([save, rename])
    expect(d.list?.A).toMatchObject({ display_name: 'Menu name', scenes: [] })
    expect(d.log).toEqual(['put A A', 'rename A Menu name'])      // rename ordered after the save
    expect(d.daemon.A.display_name).toBe('Menu name')
  })

  it('loads only after queued writes landed', async () => {
    const { d, p } = setup()
    d.hold()
    void p.save('A', mkPreset('A1'))
    void p.save('A', mkPreset('A2'))
    const loaded = p.load('A')
    await tick()
    expect(d.transport.gets).toBe(0)
    d.release()
    expect((await loaded).display_name).toBe('A2')
  })

  it('updates read-modify-write after pending saves', async () => {
    const { d, p } = setup()
    d.hold()
    void p.save('A', { ...mkPreset('A'), scenes: [] })
    const updated = p.update('A', preset => ({ ...preset, scenes: [...preset.scenes, { uuid: 'n', type: 'x', arguments: {} }] }))
    await tick()
    d.release()
    expect((await updated).scenes.map(s => s.uuid)).toEqual(['n'])   // based on the saved draft, not the old daemon state
    expect(d.list?.A.scenes.map(s => s.uuid)).toEqual(['n'])
  })

  it('keeps queued drafts when a delete fails, and orders a rename after them', async () => {
    const { d, p } = setup()
    d.hold()
    const first = p.save('A', mkPreset('d1'))
    await tick()
    const second = p.save('A', mkPreset('d2'))
    d.failNext('remove')
    const removal = p.remove('A')
    expect(p.canWrite('A')).toBe(false)
    d.release()
    await expect(removal).rejects.toThrow()
    expect(p.canWrite('A')).toBe(true)
    const rename = p.rename('A', 'r')
    await Promise.all([first, second, rename])
    expect(d.log).toEqual(['put A d1', 'remove A', 'put A d2', 'rename A r'])
  })

  it('drops held drafts once deleted, and writes again after the preset is restored', async () => {
    const { d, p } = setup()
    d.hold()
    const inFlight = p.save('B', mkPreset('b1'))
    await tick()
    const held = p.save('B', mkPreset('b2'))
    const removal = p.remove('B')
    d.release()
    await Promise.all([inFlight, held, removal])
    expect(d.log).toEqual(['put B b1', 'remove B'])
    expect(p.canWrite('B')).toBe(false)
    const later = p.save('B', mkPreset('later'))                  // held while the preset is gone
    await tick()
    expect(d.log).toHaveLength(2)
    p.listed({ ...d.list, B: mkPreset('B') })                     // restored through /add_preset?id=B
    await later
    expect(d.log.at(-1)).toBe('put B later')
  })

  it('runs a save queued from a completion handler of the previous save', async () => {
    const { d, p } = setup()
    await p.save('A', mkPreset('r1')).then(() => p.save('A', mkPreset('r2')))
    expect(d.puts.map(x => x.preset.display_name)).toEqual(['r1', 'r2'])
  })
})
