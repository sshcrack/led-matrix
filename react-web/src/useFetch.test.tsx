import type { ReactNode } from 'react'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { act, cleanup, renderHook, waitFor } from '@testing-library/react'
import useFetch from './useFetch'
import { ApiUrlContext } from './components/apiUrl/apiUrlContext'

const API = 'http://matrix.test'
const wrapper = ({ children }: { children: ReactNode }) => <ApiUrlContext.Provider value={API}>{children}</ApiUrlContext.Provider>

const fetchMock = vi.fn<typeof fetch>()

beforeEach(() => {
  fetchMock.mockReset()
  vi.stubGlobal('fetch', fetchMock)
})

afterEach(() => {
  cleanup()
  vi.useRealTimers()
  vi.unstubAllGlobals()
})

describe('useFetch', () => {
  it('loads data from apiUrl + path', async () => {
    fetchMock.mockResolvedValue(new Response(JSON.stringify({ a: 1 })))
    const { result } = renderHook(() => useFetch<{ a: number }>('/thing'), { wrapper })
    expect(result.current.isLoading).toBe(true)
    await waitFor(() => expect(result.current.isLoading).toBe(false))
    expect(result.current.data).toEqual({ a: 1 })
    expect(result.current.error).toBeNull()
    expect(fetchMock.mock.calls[0][0]).toBe(`${API}/thing`)
  })

  it('reports errors and clears data', async () => {
    fetchMock.mockRejectedValue(new Error('boom'))
    const { result } = renderHook(() => useFetch('/thing'), { wrapper })
    await waitFor(() => expect(result.current.isLoading).toBe(false))
    expect(result.current.error?.message).toBe('boom')
    expect(result.current.data).toBeNull()
  })

  it('reports invalid JSON as an error', async () => {
    fetchMock.mockResolvedValue(new Response('not json'))
    const { result } = renderHook(() => useFetch('/thing'), { wrapper })
    await waitFor(() => expect(result.current.isLoading).toBe(false))
    expect(result.current.error).toBeInstanceOf(Error)
  })

  it('stays loading and does not fetch without an api url', () => {
    const { result } = renderHook(() => useFetch('/thing'), {
      wrapper: ({ children }: { children: ReactNode }) => <ApiUrlContext.Provider value={null}>{children}</ApiUrlContext.Provider>,
    })
    expect(result.current.isLoading).toBe(true)
    expect(fetchMock).not.toHaveBeenCalled()
  })

  it('aborts the request when the timeout elapses', async () => {
    vi.useFakeTimers()
    fetchMock.mockImplementation((_url, init) => new Promise((_resolve, reject) => {
      init?.signal?.addEventListener('abort', () => reject(new DOMException('aborted', 'AbortError')))
    }))
    const { result } = renderHook(() => useFetch('/slow', 1000), { wrapper })
    expect(result.current.isLoading).toBe(true)
    await act(async () => { await vi.advanceTimersByTimeAsync(1000) })
    expect(result.current.isLoading).toBe(false)
    expect(result.current.error?.name).toBe('AbortError')
  })

  it('aborts the in-flight request on unmount', () => {
    let signal: AbortSignal | undefined
    fetchMock.mockImplementation((_url, init) => {
      signal = init?.signal ?? undefined
      return new Promise(() => {})
    })
    const { unmount } = renderHook(() => useFetch('/thing'), { wrapper })
    expect(signal?.aborted).toBe(false)
    unmount()
    expect(signal?.aborted).toBe(true)
  })

  it('refetches with loading state when setRetry is called', async () => {
    fetchMock.mockResolvedValueOnce(new Response(JSON.stringify(1))).mockResolvedValueOnce(new Response(JSON.stringify(2)))
    const { result } = renderHook(() => useFetch<number>('/n'), { wrapper })
    await waitFor(() => expect(result.current.data).toBe(1))
    act(() => result.current.setRetry((n) => n + 1))
    expect(result.current.isLoading).toBe(true)
    await waitFor(() => expect(result.current.data).toBe(2))
    expect(result.current.isLoading).toBe(false)
    expect(fetchMock).toHaveBeenCalledTimes(2)
  })
})
