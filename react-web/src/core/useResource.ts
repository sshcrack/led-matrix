import { useCallback, useEffect, useRef, useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'

/**
 * GET a JSON resource and keep it fresh without flicker.
 *
 * Unlike `useFetch`, `refresh()` keeps the previous data on screen while the
 * request runs, and `pollMs` re-fetches while the tab is visible so several
 * clients (phone + desktop) stay in sync.
 */
export function useResource<T>(path: string | null, opts: { pollMs?: number } = {}) {
  const apiUrl = useApiUrl()
  const [data, setData] = useState<T | null>(null)
  const [error, setError] = useState<Error | null>(null)
  const [loaded, setLoaded] = useState(false)
  const seq = useRef(0)

  const load = useCallback(async (signal?: AbortSignal) => {
    if (!apiUrl || !path) return
    const mine = ++seq.current
    try {
      const res = await fetch(apiUrl + path, { signal })
      if (!res.ok) throw new Error(`${path} → ${res.status}`)
      const json = (await res.json()) as T
      if (mine === seq.current) { setData(json); setError(null) }
    } catch (e) {
      if ((e as Error).name === 'AbortError') return
      if (mine === seq.current) setError(e as Error)
    } finally {
      if (mine === seq.current) setLoaded(true)
    }
  }, [apiUrl, path])

  useEffect(() => {
    const controller = new AbortController()
    // eslint-disable-next-line react-hooks/set-state-in-effect -- initial fetch on mount / path change
    void load(controller.signal)
    return () => controller.abort()
  }, [load])

  const pollMs = opts.pollMs
  useEffect(() => {
    if (!pollMs) return
    const id = window.setInterval(() => { if (document.visibilityState === 'visible') void load() }, pollMs)
    return () => window.clearInterval(id)
  }, [pollMs, load])

  const refresh = useCallback(() => load(), [load])
  return { data, error, loading: !loaded, refresh, setData }
}
