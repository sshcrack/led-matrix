import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { act, renderHook, waitFor } from '@testing-library/react'
import type { ReactNode } from 'react'
import { ApiUrlContext } from '~/components/apiUrl/apiUrlContext'
import { MatrixContext } from './matrixContext'
import { usePresetEditor } from './usePresetEditor'
import { enqueueSave } from './presetSaveQueue'
import type { RawPreset } from '~/apiTypes/list_presets'

const mkPreset = (name: string): RawPreset => ({
  display_name: name, transition_name: 'blend', transition_duration: 0,
  scenes: [{ uuid: 'u', type: 'clock', arguments: {} }],
}) as RawPreset

describe('usePresetEditor', () => {
  const saved: { id: string; preset: RawPreset }[] = []
  let shared: Record<string, RawPreset>
  let release: (() => void) | null = null
  let holdFirstSave = false

  // `send` is the network call; `savePreset` goes through the real shared queue like useMatrix does.
  const send = vi.fn(async (id: string, preset: RawPreset) => {
    if (holdFirstSave) { holdFirstSave = false; await new Promise<void>(r => { release = r }) }
    saved.push({ id, preset })
    shared = { ...shared, [id]: preset }   // the daemon now holds it
  })
  const savePreset = (id: string, preset: RawPreset) => enqueueSave(id, preset, d => send(id, d))

  const wrapper = ({ children }: { children: ReactNode }) => (
    <ApiUrlContext.Provider value="http://x">
      <MatrixContext.Provider value={{ savePreset, presets: shared } as never}>{children}</MatrixContext.Provider>
    </ApiUrlContext.Provider>
  )

  beforeEach(() => {
    saved.length = 0; send.mockClear(); release = null
    shared = { A: mkPreset('A'), B: mkPreset('B') }
    vi.stubGlobal('fetch', vi.fn(async (url: string) => {
      const id = new URL(url).searchParams.get('id')!
      return { ok: true, json: async () => structuredClone(shared[id]) }
    }))
  })
  afterEach(() => vi.unstubAllGlobals())

  it('never saves one preset’s draft under another preset’s id', async () => {
    holdFirstSave = true
    const { result, rerender } = renderHook(({ id }) => usePresetEditor(id, 5), { wrapper, initialProps: { id: 'A' } })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('A'))
    act(() => result.current.setName('A edited'))
    await waitFor(() => expect(send).toHaveBeenCalledTimes(1))   // A's save is now in flight
    rerender({ id: 'B' })                                               // switch before it completes
    await act(async () => { release?.() })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('B'))
    await new Promise(r => setTimeout(r, 30))
    expect(saved.every(s => (s.id === 'A') === (s.preset.display_name === 'A edited'))).toBe(true)
    expect(saved.find(s => s.id === 'B')).toBeUndefined()
  })

  it('keeps a rename made elsewhere when a scene is edited afterwards', async () => {
    const { result, rerender } = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('A'))
    shared = { ...shared, A: { ...shared.A, display_name: 'Renamed in menu' } }   // rail-menu rename
    rerender()
    await waitFor(() => expect(result.current.preset?.display_name).toBe('Renamed in menu'))
    act(() => result.current.removeScene('u'))
    await waitFor(() => expect(saved.length).toBe(1))
    expect(saved[0].preset.display_name).toBe('Renamed in menu')
  })

  it('does not overwrite a name fetched newer than the shared list', async () => {
    const stale = { ...shared }                                   // list still shows "A"
    shared = { ...shared, A: { ...shared.A, display_name: 'Renamed elsewhere' } }
    const staleWrapper = ({ children }: { children: ReactNode }) => (
      <ApiUrlContext.Provider value="http://x">
        <MatrixContext.Provider value={{ savePreset, presets: stale } as never}>{children}</MatrixContext.Provider>
      </ApiUrlContext.Provider>
    )
    const { result } = renderHook(() => usePresetEditor('A', 5), { wrapper: staleWrapper })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('Renamed elsewhere'))
    act(() => result.current.removeScene('u'))
    await waitFor(() => expect(saved.length).toBe(1))
    expect(saved[0].preset.display_name).toBe('Renamed elsewhere')
  })

  it('lets a menu rename win again after an inline rename was saved', async () => {
    const view = renderHook(() => usePresetEditor('A', 5), { wrapper: ({ children }: { children: ReactNode }) => (
      <ApiUrlContext.Provider value="http://x">
        <MatrixContext.Provider value={{ savePreset, presets: shared } as never}>{children}</MatrixContext.Provider>
      </ApiUrlContext.Provider>) })
    await waitFor(() => expect(view.result.current.preset?.display_name).toBe('A'))
    holdFirstSave = true
    act(() => view.result.current.setName('B'))
    await waitFor(() => expect(send).toHaveBeenCalledTimes(1))        // save of "B" in flight
    act(() => view.result.current.removeScene('u'))                          // edit while saving
    await act(async () => { release?.() })                                   // "B" acknowledged
    await waitFor(() => expect(saved.length).toBeGreaterThanOrEqual(2))      // follow-up save of the scene edit
    shared = { ...shared, A: { ...shared.A, display_name: 'C' } }            // menu rename
    view.rerender()
    await waitFor(() => expect(view.result.current.preset?.display_name).toBe('C'))
  })

  it('does not retry failed saves in a tight loop and never saves a deleted preset', async () => {
    send.mockRejectedValueOnce(new Error('quota'))
    const { result } = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(result.current.preset?.display_name).toBe('A'))
    Object.defineProperty(document, 'visibilityState', { value: 'hidden', configurable: true })
    act(() => result.current.removeScene('u'))
    await act(async () => { document.dispatchEvent(new Event('visibilitychange')) })
    await new Promise(r => setTimeout(r, 40))
    expect(send.mock.calls.length).toBeLessThanOrEqual(2)
    expect(result.current.saveState).toBe('error')
    Object.defineProperty(document, 'visibilityState', { value: 'visible', configurable: true })

    const { beginPresetDelete, finishPresetDelete, reconcilePresetQueue } = await import('./presetSaveQueue')
    await beginPresetDelete('B'); finishPresetDelete('B', true)
    const b = renderHook(() => usePresetEditor('B', 5), { wrapper })
    await waitFor(() => expect(b.result.current.preset?.display_name).toBe('B'))
    send.mockClear()
    act(() => b.result.current.removeScene('u'))
    b.unmount()
    await new Promise(r => setTimeout(r, 30))
    expect(send).not.toHaveBeenCalled()
    reconcilePresetQueue(['B'])   // restored through /add_preset?id=B: saving works again
    const again = renderHook(() => usePresetEditor('B', 5), { wrapper })
    await waitFor(() => expect(again.result.current.preset?.display_name).toBe('B'))
    act(() => again.result.current.removeScene('u'))
    await waitFor(() => expect(send).toHaveBeenCalledTimes(1))
  })

  it('a reopened editor loads only after the previous editor’s queued saves landed', async () => {
    holdFirstSave = true
    const first = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await waitFor(() => expect(first.result.current.preset?.display_name).toBe('A'))
    act(() => first.result.current.setName('A1'))
    await waitFor(() => expect(send).toHaveBeenCalledTimes(1))                   // save "A1" in flight (held)
    act(() => first.result.current.setName('A2'))                                 // edit while saving
    first.unmount()                                                               // close: A2 queued behind A1
    const fetchCalls = (fetch as unknown as ReturnType<typeof vi.fn>).mock.calls.length
    const second = renderHook(() => usePresetEditor('A', 5), { wrapper })
    await new Promise(r => setTimeout(r, 30))
    expect((fetch as unknown as ReturnType<typeof vi.fn>).mock.calls.length).toBe(fetchCalls)   // still waiting: no stale GET
    await act(async () => { release?.() })
    await waitFor(() => expect(saved.map(s => s.preset.display_name)).toEqual(['A1', 'A2']))
    await waitFor(() => expect(second.result.current.preset?.display_name).toBe('A2'))
  })

  it('keeps queued drafts when a delete fails, and orders a rename after them', async () => {
    const { enqueueSave, enqueueOp, beginPresetDelete, finishPresetDelete } = await import('./presetSaveQueue')
    const order: string[] = []
    let gate: () => void = () => {}
    const first = enqueueSave('Q', mkPreset('d1'), async d => { await new Promise<void>(r => { gate = r }); order.push(d.display_name!) })
    const second = enqueueSave('Q', mkPreset('d2'), async d => { order.push(d.display_name!) })
    const del = beginPresetDelete('Q')
    gate(); await del
    expect(order).toEqual(['d1'])                                                 // d2 held back, not dropped
    finishPresetDelete('Q', false)                                                // DELETE failed: resume
    const rename = enqueueOp('Q', async () => { order.push('rename') })
    await Promise.all([first, second, rename])
    expect(order).toEqual(['d1', 'd2', 'rename'])
  })

  it('runs a save queued from a completion handler of the previous save', async () => {
    const { enqueueSave } = await import('./presetSaveQueue')
    const order: string[] = []
    await enqueueSave('R', mkPreset('r1'), async d => { order.push(d.display_name!) })
      .then(() => enqueueSave('R', mkPreset('r2'), async d => { order.push(d.display_name!) }))
    expect(order).toEqual(['r1', 'r2'])
  })
})
