import { describe, expect, it } from 'vitest'
import { initialSetupState, messages, runningAction, setupReducer, type SetupEvent, type SetupState, type SpotifySetupStatus } from './spotifySetupModel'

const base: SpotifySetupStatus = { configured: true, connected: false, client_id: 'id', redirect_uri: 'https://relay/callback' }
const run = (events: SetupEvent[], from: SetupState = initialSetupState) => events.reduce(setupReducer, from)
const loaded = (status: Partial<SpotifySetupStatus> = {}): SetupEvent => ({ type: 'statusLoaded', status: { ...base, ...status } })
const signingIn = run([loaded(), { type: 'actionStarted', action: 'connect' }, { type: 'signInOpened', attempt: 3 }])

describe('Spotify setup state machine', () => {
  it('loads, or reports a missing plugin', () => {
    expect(run([loaded()]).availability).toBe('available')
    expect(run([{ type: 'statusMissing' }])).toMatchObject({ availability: 'unavailable', status: null })
  })

  it('shows a spinner only for the running action', () => {
    const busy = run([loaded(), { type: 'actionStarted', action: 'save' }])
    expect(runningAction(busy)).toBe('save')
    expect(runningAction(run([{ type: 'saved' }], busy))).toBeNull()
    expect(run([{ type: 'saved' }], busy).feedback?.text).toBe(messages.saved)
  })

  it('ends the sign-in with a confirmation once the Pi reports this attempt connected', () => {
    expect(signingIn.feedback?.text).toBe(messages.waiting)
    const still = run([loaded({ login: { attempt: 3, phase: 'waiting' } })], signingIn)
    expect(still.activity.kind).toBe('signingIn')
    const done = run([loaded({ connected: true, login: { attempt: 3, phase: 'connected' } })], still)
    expect(done.activity.kind).toBe('idle')
    expect(done.feedback).toMatchObject({ tone: 'notice', text: messages.connected })
  })

  it('a reconnect waits for its own attempt even though Spotify was already connected', () => {
    const waiting = run([loaded({ connected: true, login: { attempt: 2, phase: 'connected' } })], signingIn)
    expect(waiting.activity.kind).toBe('signingIn')
  })

  it('shows the Pi message when the attempt was declined or expired', () => {
    const declined = run([loaded({ login: { attempt: 3, phase: 'failed', error: 'Spotify access was declined.' } })], signingIn)
    expect(declined.activity.kind).toBe('idle')
    expect(declined.feedback).toMatchObject({ tone: 'error', text: 'Spotify access was declined.' })
    const expired = run([loaded({ login: { attempt: 3, phase: 'expired' } })], signingIn)
    expect(expired.feedback).toMatchObject({ tone: 'error', text: messages.expired })
  })

  it('stops waiting when another tab started a newer attempt', () => {
    const superseded = run([loaded({ login: { attempt: 4, phase: 'waiting' } })], signingIn)
    expect(superseded.activity.kind).toBe('idle')
    expect(superseded.feedback?.text).toBe(messages.superseded)
  })

  it('falls back to the connected flag on daemons without login reports', () => {
    const legacy = run([loaded(), { type: 'actionStarted', action: 'connect' }, { type: 'signInOpened', attempt: null }])
    expect(run([loaded()], legacy).activity.kind).toBe('signingIn')
    expect(run([loaded({ connected: true })], legacy).feedback?.text).toBe(messages.connected)
  })

  it('times out a sign-in nobody finished, and ignores a late timeout', () => {
    const timedOut = run([{ type: 'signInTimedOut' }], signingIn)
    expect(timedOut).toMatchObject({ activity: { kind: 'idle' }, feedback: { tone: 'error', text: messages.expired } })
    const idle = run([loaded()])
    expect(run([{ type: 'signInTimedOut' }], idle)).toBe(idle)
  })

  it('clears a failed status check on the next good one, but keeps action results', () => {
    const checkFailed = run([loaded(), { type: 'statusFailed', message: 'offline' }])
    expect(checkFailed.feedback).toMatchObject({ tone: 'error', text: 'offline' })
    expect(run([loaded()], checkFailed).feedback).toBeNull()

    const saveFailed = run([loaded(), { type: 'actionStarted', action: 'save' }, { type: 'actionFailed', message: 'not writable' }])
    expect(run([{ type: 'statusFailed', message: 'offline' }, loaded()], saveFailed).feedback?.text).toBe('not writable')
  })
})
