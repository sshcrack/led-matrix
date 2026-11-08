import { useCallback, useEffect, useReducer, useRef } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { toast } from 'sonner'
import type { UpdateStatus } from '~/apiTypes/update'
import { INSTALL_TIMEOUT_MS, installReducer, isFollowing, type Installation } from '~/components/updates/updateProgress'

const POLL_MS = 2000

interface Options {
  /** Each status the Pi answers with while an install is followed (the page shows it in place). */
  onStatus?: (status: UpdateStatus) => void
  /** Re-read the page's status (after a check, and once an install has an outcome). */
  onRefresh?: () => void
}

/**
 * Starts installs and follows them until the Pi reports the outcome (transitions: `updateProgress.ts`).
 * Polling is the hook's own, so the restart window (daemon not answering) never blanks the page's status.
 */
export function useUpdateInstallation({ onStatus, onRefresh }: Options = {}) {
  const apiUrl = useApiUrl()
  const [installation, dispatch] = useReducer(installReducer, { kind: 'idle' } as Installation)
  const following = isFollowing(installation)
  const callbacks = useRef({ onStatus, onRefresh })
  useEffect(() => { callbacks.current = { onStatus, onRefresh } })

  // One request at a time, so statuses arrive in order; a failed request means the daemon is restarting.
  useEffect(() => {
    if (!following || !apiUrl) return
    let cancelled = false
    let timer: number | undefined
    let controller: AbortController | undefined
    const poll = async () => {
      controller = new AbortController()
      const abort = window.setTimeout(() => controller?.abort(), POLL_MS * 2)
      try {
        const res = await fetch(`${apiUrl}/api/update/status`, { signal: controller.signal, cache: 'no-store' })
        if (!res.ok) throw new Error(`HTTP ${res.status}`)
        const status = await res.json() as UpdateStatus
        if (cancelled) return
        callbacks.current.onStatus?.(status)
        dispatch({ type: 'statusLoaded', status })
      } catch {
        if (!cancelled) dispatch({ type: 'unreachable' })
      } finally {
        window.clearTimeout(abort)
        if (!cancelled) timer = window.setTimeout(() => { void poll() }, POLL_MS)
      }
    }
    void poll()
    return () => { cancelled = true; window.clearTimeout(timer); controller?.abort() }
  }, [following, apiUrl])

  // One deadline for the whole install, not per phase.
  const active = installation.kind === 'requesting' || following
  useEffect(() => {
    if (!active) return
    const timer = window.setTimeout(() => dispatch({ type: 'timedOut' }), INSTALL_TIMEOUT_MS)
    return () => window.clearTimeout(timer)
  }, [active])

  // Refresh the page's own status once the install has an outcome.
  const outcome = installation.kind === 'done' || installation.kind === 'failed' ? installation.kind : null
  useEffect(() => {
    if (!outcome) return
    callbacks.current.onRefresh?.()
    if (outcome === 'done') toast.success('Update installed')
  }, [outcome])

  const checkForUpdates = useCallback(async () => {
    if (!apiUrl) return
    try {
      const res = await fetch(`${apiUrl}/api/update/check`, { method: 'POST' })
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      if (data.update_available) {
        toast.success(`Update available: ${data.version || 'new version'}`)
      } else {
        toast.info('Already on latest version')
      }
      callbacks.current.onRefresh?.()
    } catch {
      toast.error('Failed to check for updates')
    }
  }, [apiUrl])

  const installUpdate = useCallback(async (version: string) => {
    if (!apiUrl) return
    dispatch({ type: 'requested', target: version })
    try {
      const res = await fetch(`${apiUrl}/api/update/install?version=${encodeURIComponent(version)}`, { method: 'POST' })
      // 409: an install is already running (another tab, or the automatic updater); follow that one.
      if (!res.ok && res.status !== 409) throw new Error((await res.text().catch(() => '')) || `HTTP ${res.status}`)
      dispatch({ type: 'accepted' })
    } catch (e) {
      const message = (e as Error).message
      dispatch({ type: 'rejected', message: `Could not start the update: ${message}` })
      toast.error(`Failed to install update: ${message}`)
    }
  }, [apiUrl])

  return { installation, checkForUpdates, installUpdate }
}
