import { afterEach, describe, expect, it, vi } from 'vitest'
import { act, renderHook } from '@testing-library/react'
import type { ReactNode } from 'react'
import { ApiUrlContext } from '~/components/apiUrl/apiUrlContext'
import type { UpdateStatus } from '~/apiTypes/update'
import { useUpdateInstallation } from './useUpdateInstallation'

vi.mock('sonner', () => ({ toast: { success: vi.fn(), error: vi.fn(), info: vi.fn() } }))

const wrapper = ({ children }: { children: ReactNode }) => <ApiUrlContext.Provider value="http://matrix.test">{children}</ApiUrlContext.Provider>
const status = (code: number, version = '1.0.0'): UpdateStatus => ({
  auto_update_enabled: true, check_interval_hours: 24, current_version: version, latest_version: '1.1.0',
  update_available: true, status: code, error_message: '',
})
const json = (data: unknown, code = 200) => Promise.resolve(new Response(JSON.stringify(data), { status: code }))

afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals() })

describe('useUpdateInstallation', () => {
  it('keeps polling through the restart until the Pi reports success, without blanking the page status', async () => {
    vi.useFakeTimers()
    // POST install, then status polls: downloading, two failures while the daemon restarts, success on 1.1.0.
    const replies = [
      () => json({ status: 'started' }, 202),
      () => json(status(2)),
      () => Promise.reject(new TypeError('Failed to fetch')),
      () => Promise.reject(new TypeError('Failed to fetch')),
      () => json(status(5, '1.1.0')),
    ]
    const fetcher = vi.fn().mockImplementation(() => (replies.shift() ?? (() => json(status(5, '1.1.0'))))())
    vi.stubGlobal('fetch', fetcher)
    const seen: UpdateStatus[] = []
    const onRefresh = vi.fn()
    const { result } = renderHook(() => useUpdateInstallation({ onStatus: s => seen.push(s), onRefresh }), { wrapper })

    await act(() => result.current.installUpdate('1.1.0'))
    await act(() => vi.advanceTimersByTimeAsync(0))
    expect(result.current.installation).toMatchObject({ kind: 'running', phase: 'downloading' })
    await act(() => vi.advanceTimersByTimeAsync(2000))
    expect(result.current.installation.kind).toBe('restarting')
    await act(() => vi.advanceTimersByTimeAsync(4000))
    expect(result.current.installation).toEqual({ kind: 'done', version: '1.1.0' })
    expect(seen.map(s => s.status)).toEqual([2, 5])
    expect(onRefresh).toHaveBeenCalledTimes(1)

    const calls = fetcher.mock.calls.length
    await act(() => vi.advanceTimersByTimeAsync(10_000))
    expect(fetcher.mock.calls.length).toBe(calls) // polling stops with the outcome
  })

  it('follows an install that is already running instead of failing on 409', async () => {
    vi.useFakeTimers()
    vi.stubGlobal('fetch', vi.fn()
      .mockImplementationOnce(() => json({ status: 'already_running' }, 409))
      .mockImplementation(() => json(status(3))))
    const { result } = renderHook(() => useUpdateInstallation(), { wrapper })
    await act(() => result.current.installUpdate('1.1.0'))
    await act(() => vi.advanceTimersByTimeAsync(0))
    expect(result.current.installation).toMatchObject({ kind: 'running', phase: 'installing' })
  })
})
