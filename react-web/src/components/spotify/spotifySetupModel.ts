/**
 * The Spotify connection panel as one state machine (pure, so it is tested without React).
 *
 *   loading ─status─▶ ready ─┬─ save ──▶ busy(save) ──▶ ready (+notice | +error)
 *           ─404───▶ unavailable                 └─ connect ─▶ busy(connect) ─▶ signingIn(attempt) ─┐
 *                                                                                              │
 *   signingIn: status polls report the Pi's `login` for *this* attempt:                        │
 *     connected ─▶ ready + "Spotify connected"   failed/expired ─▶ ready + the Pi's message ◀───┘
 *     10 minutes without an answer ─▶ ready + "sign-in expired"
 *
 * Older daemons send no `login`; then `connected` turning true ends the sign-in, as before.
 * Feedback remembers where it came from, so a successful status check clears a status-check error
 * but never hides the outcome of something the user did.
 */

export type LoginPhase = 'idle' | 'waiting' | 'exchanging' | 'connected' | 'failed' | 'expired'

export interface SpotifySetupStatus {
  configured: boolean
  connected: boolean
  client_id: string
  redirect_uri: string
  /** Latest sign-in attempt on the Pi (absent on daemons before this field existed). */
  login?: { attempt: number; phase: LoginPhase; error?: string }
}

export type SetupAction = 'save' | 'connect' | 'complete'

export type Activity =
  | { kind: 'idle' }
  | { kind: 'busy'; action: SetupAction }
  /** The Spotify tab is open; `attempt` is the Pi's id for it (null on older daemons). */
  | { kind: 'signingIn'; attempt: number | null }

export interface Feedback {
  tone: 'error' | 'notice'
  text: string
  /** `check` = a status request; `action` = something the user started. */
  source: 'check' | 'action'
}

export interface SetupState {
  availability: 'loading' | 'available' | 'unavailable'
  status: SpotifySetupStatus | null
  activity: Activity
  feedback: Feedback | null
}

export type SetupEvent =
  | { type: 'statusLoaded'; status: SpotifySetupStatus }
  | { type: 'statusMissing' }
  | { type: 'statusFailed'; message: string }
  | { type: 'actionStarted'; action: SetupAction }
  | { type: 'actionFailed'; message: string }
  | { type: 'saved' }
  | { type: 'signInOpened'; attempt: number | null }
  | { type: 'completed' }
  | { type: 'signInTimedOut' }

export const SIGN_IN_TIMEOUT_MS = 10 * 60 * 1000

export const messages = {
  saved: 'Client ID saved. Connect your Spotify account next.',
  waiting: 'Finish signing in in the Spotify tab. You will be sent back to your matrix automatically.',
  connected: 'Spotify connected. Play music in Spotify to bring it to your matrix.',
  expired: 'The Spotify sign-in expired. Choose Connect Spotify to try again.',
  failed: 'Spotify did not connect. Choose Connect Spotify to try again.',
  superseded: 'A newer Spotify sign-in was started from another tab or device. Finish it there, or choose Connect Spotify here.',
} as const

export const initialSetupState: SetupState = { availability: 'loading', status: null, activity: { kind: 'idle' }, feedback: null }

const idle: Activity = { kind: 'idle' }
const notice = (text: string): Feedback => ({ tone: 'notice', text, source: 'action' })
const error = (text: string, source: Feedback['source'] = 'action'): Feedback => ({ tone: 'error', text, source })

/** How a status report ends the running sign-in, or null while it is still open. */
function signInOutcome(attempt: number | null, status: SpotifySetupStatus): Feedback | null {
  const login = status.login
  if (attempt === null || !login) return status.connected ? notice(messages.connected) : null
  if (login.attempt !== attempt) return login.attempt > attempt ? notice(messages.superseded) : null
  switch (login.phase) {
    case 'connected': return notice(messages.connected)
    case 'failed': return error(login.error || messages.failed)
    case 'expired': return error(login.error || messages.expired)
    default: return null
  }
}

export function setupReducer(state: SetupState, event: SetupEvent): SetupState {
  switch (event.type) {
    case 'statusLoaded': {
      const next: SetupState = {
        ...state,
        availability: 'available',
        status: event.status,
        feedback: state.feedback?.source === 'check' ? null : state.feedback,
      }
      if (state.activity.kind !== 'signingIn') return next
      const outcome = signInOutcome(state.activity.attempt, event.status)
      return outcome ? { ...next, activity: idle, feedback: outcome } : next
    }
    case 'statusMissing':
      return { ...state, availability: 'unavailable', status: null, activity: idle }
    case 'statusFailed':
      // Never replace what an action reported with a background check failure.
      return state.feedback?.source === 'action' ? state : { ...state, feedback: error(event.message, 'check') }
    case 'actionStarted':
      return { ...state, activity: { kind: 'busy', action: event.action }, feedback: null }
    case 'actionFailed':
      return { ...state, activity: idle, feedback: error(event.message) }
    case 'saved':
      return { ...state, activity: idle, feedback: notice(messages.saved) }
    case 'signInOpened':
      return { ...state, activity: { kind: 'signingIn', attempt: event.attempt }, feedback: notice(messages.waiting) }
    case 'completed':
      return { ...state, activity: idle, feedback: notice(messages.connected) }
    case 'signInTimedOut':
      return state.activity.kind === 'signingIn' ? { ...state, activity: idle, feedback: error(messages.expired) } : state
  }
}

/** The action whose button shows a spinner, if any. */
export function runningAction(state: SetupState): SetupAction | null {
  return state.activity.kind === 'busy' ? state.activity.action : null
}
