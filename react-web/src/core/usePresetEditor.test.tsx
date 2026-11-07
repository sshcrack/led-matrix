import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { act, renderHook, waitFor } from '@testing-library/react'
import type { ReactNode } from 'react'
import { MatrixContext } from './matrixContext'
import { usePresetEditor } from './usePresetEditor'
import { createPresetPersistence, type PresetPersistence } from './presetPersistence'
import { createTestPresetDaemon, mkPreset } from './presetTestTransport'

// The editor runs against the real preset persistence; only the daemon is in memory.
describe('usePresetEditor', () => {
  let d: ReturnType<typeof createTestPresetDaemon>
  let persistence: PresetPersistence
  const wrapper = ({ children }: { children: ReactNode }) => (
    <MatrixContext.Provider value={{ persistence, presets: d.list } as never}>{children}</MatrixContext.Provider>
  )
  const names = () => d.puts.map(s => s.preset.display_name)

  beforeEach(() => {
    d = createTestPresetDaemon({ A: mkPreset('A'), B: mkPreset('B') })
    persistence = createPresetPersistence(d.transport, d.writeList)
    d.attach(persistence.listed)
  })
  afterEach(() => { Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true }) })

  it('never saves one preset’s draft under another preset’s id', async () => {
    const { result, rerender } = renderHook(({ id }) => usePresetEditor(id, 5), { wrapper, initialProps: { id: 'A' } })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('A'))
    d.hold()
    act(() => result.current.setName('A edited'))
    await waitFor(() => expect(d.log).toEqual(['put A A edited']))   // A's save is now in flight
    rerender({ id: 'B' })                                              // switch before it completes
    await act(async () => { d.release() })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('B'))
    await new Promise(r => setTimeout(r, 30))
    expect(d.puts.every(s => (s.id === 'A') === (s.preset.display_name === 'A edited'))).toBe(true)
    expect(d.puts.find(s => s.id === 'B')).toBeUndefined()
  })

  it('keeps a menu rename when a scene is edited afterwards', async () => {
    const { result, rerender } = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('A'))
    await act(() => persistence.rename('A', 'Renamed in menu'))
    rerender()
    await waitFor(() => expect(result.current.preset?.display_name).toBe('Renamed in menu'))
    act(() => result.current.removeScene('u'))
    await waitFor(() => expect(d.puts.length).toBe(1))
    expect(d.puts[0].preset.display_name).toBe('Renamed in menu')
  })

  it('does not overwrite a name fetched newer than the shared list', async () => {
    d.daemon.A = { ...d.daemon.A, display_name: 'Renamed elsewhere' }   // list still shows "A"
    const { result } = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('Renamed elsewhere'))
    act(() => result.current.removeScene('u'))
    await waitFor(() => expect(d.puts.length).toBe(1))
    expect(d.puts[0].preset.display_name).toBe('Renamed elsewhere')
  })

  it('lets a menu rename win again after an inline rename was saved', async () => {
    const view = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(view.result.current.preset?.display_name).toBe('A'))
    d.hold()
    act(() => view.result.current.setName('B'))
    await waitFor(() => expect(d.log).toHaveLength(1))                  // save of "B" in flight
    act(() => view.result.current.removeScene('u'))                     // edit while saving
    await act(async () => { d.release() })                             // "B" acknowledged
    await waitFor(() => expect(d.puts.length).toBeGreaterThanOrEqual(2)) // follow-up save of the scene edit
    view.rerender()
    expect(d.list?.A.display_name).toBe('B')                            // acknowledged save reached the list
    await act(() => persistence.rename('A', 'C'))                       // menu rename
    view.rerender()
    await waitFor(() => expect(view.result.current.preset?.display_name).toBe('C'))
  })

  it('does not retry failed saves in a tight loop and never saves a deleted preset', async () => {
    const { result } = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('A'))
    d.failNext('put')
    Object.defineProperty(document, 'visibilityState', { value: 'hidden', configurable: true })
    act(() => result.current.removeScene('u'))
    await act(async () => { document.dispatchEvent(new Event('visibilitychange')) })
    await new Promise(r => setTimeout(r, 40))
    expect(d.log.length).toBeLessThanOrEqual(2)
    expect(result.current.saveState).toBe('error')
    Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true })

    const b = renderHook(() => usePresetEditor('B', 5), { wrapper })
    await waitFor(() => expect(b.result.current.preset?.display_name).toBe('B'))
    await act(() => persistence.remove('B'))
    d.daemon.B = mkPreset('B')                                           // the editor still has it open
    d.log.length = 0
    act(() => b.result.current.removeScene('u'))
    b.unmount()
    await new Promise(r => setTimeout(r, 30))
    expect(d.log).toEqual([])
    persistence.listed({ ...d.list, B: mkPreset('B') })                                         // restored through /add_preset?id=B: saving works again
    const again = renderHook(() => usePresetEditor('B', 5), { wrapper })
    await waitFor(() => expect(again.result.current.preset?.display_name).toBe('B'))
    act(() => again.result.current.removeScene('u'))
    await waitFor(() => expect(d.log).toEqual(['put B B']))
  })

  it('a reopened editor loads only after the previous editor’s queued saves landed', async () => {
    const first = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(first.result.current.preset?.display_name).toBe('A'))
    d.hold()
    act(() => first.result.current.setName('A1'))
    await waitFor(() => expect(d.log).toHaveLength(1))                  // save "A1" in flight (held)
    act(() => first.result.current.setName('A2'))                        // edit while saving
    first.unmount()                                                      // close: A2 queued behind A1
    const gets = d.transport.gets
    const second = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await new Promise(r => setTimeout(r, 30))
    expect(d.transport.gets).toBe(gets)                                  // still waiting: no stale GET
    await act(async () => { d.release() })
    await waitFor(() => expect(names()).toEqual(['A1', 'A2']))
    await waitFor(() => expect(second.result.current.preset?.display_name).toBe('A2'))
  })
})
