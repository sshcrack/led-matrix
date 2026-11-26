import { useEffect, useMemo } from 'react'

/**
 * Keeps an open UI on the build the daemon serves.
 *
 * The service worker (vite-plugin-pwa, `autoUpdate`) precaches the shell and its bundles, and a
 * browser only looks for a new worker on navigation, so a tab or installed PWA left open across
 * an update would keep running the old UI against the new API. We ask the worker to check for an
 * update when the daemon reports a new version, when the matrix comes back after being
 * unreachable (an update restarts it), when the page becomes visible again, and every few
 * minutes. If the served files changed, the new worker activates and `autoUpdate` reloads the page.
 */

const PERIODIC_CHECK_MS = 10 * 60_000
const RELOAD_GUARD_KEY = 'led-matrix:ui-reload-at'
const RELOAD_GUARD_MS = 30_000

let registration: ServiceWorkerRegistration | undefined

function checkForUiUpdate() {
  const reg = registration
  if (!reg || reg.installing || !navigator.onLine) return
  reg.update().catch(() => { /* offline or the daemon is restarting; a later trigger retries */ })
}

/** Called once from main.tsx with the worker registration. */
export function startUiUpdates(reg: ServiceWorkerRegistration | undefined) {
  registration = reg
  if (!reg) return
  setInterval(checkForUiUpdate, PERIODIC_CHECK_MS)
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') checkForUiUpdate()
  })
}

/**
 * A lazy chunk of the build this tab started with is gone (the daemon serves a newer build and
 * answers old chunk names with 404). Reload to pick up the new shell; the guard stops a reload
 * loop if the chunk is missing for another reason.
 */
export function installChunkErrorReload() {
  window.addEventListener('vite:preloadError', (event) => {
    let last = 0
    try { last = Number(sessionStorage.getItem(RELOAD_GUARD_KEY) ?? 0) } catch { /* storage blocked */ }
    if (Date.now() - last < RELOAD_GUARD_MS) return
    try { sessionStorage.setItem(RELOAD_GUARD_KEY, String(Date.now())) } catch { /* storage blocked */ }
    event.preventDefault()
    window.location.reload()
  })
}

export interface DaemonState { online: boolean; version: string | undefined }

/** Calls `check` when the daemon comes back online or reports a different version than before. */
export function createUpdateWatcher(check: () => void) {
  let prev: DaemonState | undefined
  return (next: DaemonState) => {
    const before = prev
    prev = next
    if (!before) return
    const cameBack = !before.online && next.online
    const versionChanged = next.version !== undefined && before.version !== undefined && next.version !== before.version
    if (cameBack || versionChanged) check()
  }
}

export function useUiUpdateTriggers(online: boolean, version: string | undefined) {
  const observe = useMemo(() => createUpdateWatcher(checkForUiUpdate), [])
  useEffect(() => { observe({ online, version }) }, [observe, online, version])
}
