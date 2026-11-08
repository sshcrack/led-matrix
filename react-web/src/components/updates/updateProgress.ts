import type { UpdateStatus } from '~/apiTypes/update'

/**
 * Names for the Pi's numeric `UpdateStatus` (shared/matrix/include/shared/matrix/update/UpdateManager.h),
 * and the browser's view of one installation from click to the new version answering.
 *
 *   idle ─install─▶ requesting ─accepted─▶ running(checking|downloading|installing) ─unreachable─▶ restarting
 *                        │                       │                                                     │
 *                        └─rejected─▶ failed ◀───┴──── status error, or 15 min without an outcome ◀──────┤
 *                                                └──── status success ──▶ done ◀─────────────────────────┘
 *
 * "The install request returned" is *not* "the update is installed": the Pi resolves the release, downloads,
 * exits, `update_service.sh` installs and health-checks the package, and only then writes the success marker
 * that the new daemon reports (or rolls back and reports an error). A new version answering before that is
 * still being health-checked, so only `success` ends the install. The Pi reports CHECKING before it answers
 * the install request, so every status read after `accepted` belongs to this install.
 */

export type ServerPhase = 'idle' | 'checking' | 'downloading' | 'installing' | 'error' | 'success' | 'disabled'

const phases: readonly ServerPhase[] = ['idle', 'checking', 'downloading', 'installing', 'error', 'success', 'disabled']

export function serverPhase(status: Pick<UpdateStatus, 'status'> | null | undefined): ServerPhase | null {
  return status ? phases[status.status] ?? null : null
}

export const isServerBusy = (phase: ServerPhase | null) => phase === 'checking' || phase === 'downloading' || phase === 'installing'

export type Installation =
  | { kind: 'idle' }
  | { kind: 'requesting'; target: string }
  | { kind: 'running'; target: string; phase: 'checking' | 'downloading' | 'installing' }
  /** The daemon stopped answering after the install started: it is replacing itself. */
  | { kind: 'restarting'; target: string }
  | { kind: 'done'; version: string }
  | { kind: 'failed'; message: string }

export type InstallEvent =
  | { type: 'requested'; target: string }
  | { type: 'accepted' }
  | { type: 'rejected'; message: string }
  | { type: 'statusLoaded'; status: UpdateStatus }
  | { type: 'unreachable' }
  | { type: 'timedOut' }

/** Longest time from click to the new daemon answering (download timeout 3 min + install + health checks). */
export const INSTALL_TIMEOUT_MS = 15 * 60 * 1000

export const isFollowing = (i: Installation): i is Extract<Installation, { kind: 'running' | 'restarting' }> =>
  i.kind === 'running' || i.kind === 'restarting'

const isActive = (i: Installation) => i.kind === 'requesting' || isFollowing(i)

export function installReducer(state: Installation, event: InstallEvent): Installation {
  switch (event.type) {
    case 'requested':
      return isActive(state) ? state : { kind: 'requesting', target: event.target }
    case 'accepted':
      return state.kind === 'requesting' ? { kind: 'running', target: state.target, phase: 'checking' } : state
    case 'rejected':
      return state.kind === 'requesting' ? { kind: 'failed', message: event.message } : state
    case 'unreachable':
      return isFollowing(state) ? { kind: 'restarting', target: state.target } : state
    case 'timedOut':
      return isActive(state)
        ? { kind: 'failed', message: 'The matrix did not finish the update in time. Check that it is powered and on the network, then reload this page.' }
        : state
    case 'statusLoaded': {
      if (!isFollowing(state)) return state
      const { status } = event
      const phase = serverPhase(status)
      if (phase === 'success') return { kind: 'done', version: status.current_version || state.target }
      if (phase === 'error') return { kind: 'failed', message: status.error_message || 'The update failed.' }
      if (state.kind === 'running' && (phase === 'checking' || phase === 'downloading' || phase === 'installing'))
        return state.phase === phase ? state : { ...state, phase }
      return state // a restarted daemon answers before the updater's health checks finish
    }
  }
}

/** One line describing the installation for the status card, or null when there is nothing to say. */
export function describeInstallation(i: Installation): { tone: 'progress' | 'success' | 'error'; text: string } | null {
  switch (i.kind) {
    case 'idle': return null
    case 'requesting': return { tone: 'progress', text: `Starting the update to ${i.target}…` }
    case 'running': return {
      tone: 'progress',
      text: i.phase === 'checking' ? `Preparing ${i.target}…` : i.phase === 'downloading' ? `Downloading ${i.target}…` : `Installing ${i.target}…`,
    }
    case 'restarting': return { tone: 'progress', text: `The matrix is restarting with ${i.target}. This page will update by itself.` }
    case 'done': return { tone: 'success', text: `Updated to ${i.version}. Reload the page to use the new web interface.` }
    case 'failed': return { tone: 'error', text: i.message }
  }
}
