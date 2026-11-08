import { useCallback, useEffect, useReducer, useRef, useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import SpotifySetup from './SpotifySetup'
import {
  initialSetupState, runningAction, setupReducer, SIGN_IN_TIMEOUT_MS,
  type SetupAction, type SpotifySetupStatus,
} from './spotifySetupModel'

const failure = (err: unknown) => err instanceof Error ? err.message : 'Spotify setup failed. Try again.'

/** The plugin owns setup; older daemons and installs without it return 404. State transitions: `spotifySetupModel.ts`. */
export default function SpotifyConnection({ settings = false }: { settings?: boolean }) {
  const apiUrl = useApiUrl()
  const dismissalKey = `spotify-setup-dismissed:${apiUrl}`
  const [dismissed, setDismissed] = useState(() => {
    try { return localStorage.getItem(dismissalKey) === '1' } catch { return false }
  })
  const [state, dispatch] = useReducer(setupReducer, initialSetupState)
  const popupRef = useRef<Window | null>(null)
  const signingIn = state.activity.kind === 'signingIn'

  const refresh = useCallback(async () => {
    if (!apiUrl) return
    try {
      const res = await fetch(`${apiUrl}/spotify/status`)
      if (res.status === 404) { dispatch({ type: 'statusMissing' }); return }
      if (!res.ok) throw new Error('Could not check Spotify connection. Try again.')
      dispatch({ type: 'statusLoaded', status: await res.json() as SpotifySetupStatus })
    } catch (err) {
      dispatch({ type: 'statusFailed', message: err instanceof Error ? err.message : 'Could not check Spotify connection.' })
    }
  }, [apiUrl])

  useEffect(() => {
    const timer = window.setTimeout(() => { void refresh() }, 0)
    return () => window.clearTimeout(timer)
  }, [refresh])
  useEffect(() => {
    const onFocus = () => { void refresh() }
    window.addEventListener('focus', onFocus)
    return () => window.removeEventListener('focus', onFocus)
  }, [refresh])
  // While the Spotify tab is open, follow the Pi's report of this attempt until it ends.
  useEffect(() => {
    if (!signingIn) return
    const timer = window.setInterval(() => { if (!document.hidden) void refresh() }, 2000)
    const expiry = window.setTimeout(() => dispatch({ type: 'signInTimedOut' }), SIGN_IN_TIMEOUT_MS)
    return () => { window.clearInterval(timer); window.clearTimeout(expiry) }
  }, [signingIn, refresh])

  async function post(path: string, body: Record<string, string> = {}) {
    const res = await fetch(`${apiUrl}/spotify/${path}`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
    })
    const data = await res.json() as { error?: string; url?: string; attempt?: number }
    if (!res.ok) throw new Error(data.error || 'Spotify setup failed. Try again.')
    return data
  }

  /** Runs one user action; its success event is dispatched by `fn`, a thrown error becomes the shown error. */
  async function act(action: SetupAction, fn: () => Promise<void>) {
    dispatch({ type: 'actionStarted', action })
    try { await fn() }
    catch (err) {
      dispatch({ type: 'actionFailed', message: failure(err) })
      throw err
    }
  }

  if (!settings && dismissed) return null
  if (!state.status) {
    if (!settings) return null
    const fetchError = state.feedback?.tone === 'error' ? state.feedback.text : null
    return <p role="status" className="text-sm text-muted-foreground">{fetchError ?? (state.availability === 'unavailable' ? 'Spotify is not installed on this matrix. Install the SpotifyScenes plugin to use this integration.' : 'Loading Spotify setup…')}</p>
  }
  const feedback = state.feedback
  return <SpotifySetup initiallyOpen={settings ? true : undefined}
    onDismiss={settings ? undefined : () => {
      setDismissed(true)
      try { localStorage.setItem(dismissalKey, '1') } catch { /* Keep dismissal for this session. */ }
    }}
    status={state.status} action={runningAction(state)} signingIn={signingIn}
    error={feedback?.tone === 'error' ? feedback.text : null}
    notice={feedback?.tone === 'notice' ? feedback.text : null}
    onRefresh={() => { void refresh() }}
    onSave={clientId => act('save', async () => {
      await post('credentials', { client_id: clientId.trim() })
      dispatch({ type: 'saved' })
      await refresh()
    })}
    onConnect={() => {
      // Open during the click so mobile popup blockers permit the sign-in tab.
      const popup = window.open('about:blank', '_blank')
      if (popup) popup.opener = null
      popupRef.current = popup
      return act('connect', async () => {
        try {
          const { url, attempt } = await post('connect', { origin: window.location.origin })
          if (!url || !url.startsWith('https://accounts.spotify.com/authorize?')) throw new Error('Spotify did not return a sign-in address.')
          if (!popup) throw new Error('Allow pop-ups for this page, then choose Connect Spotify again.')
          popup.location.href = url
          dispatch({ type: 'signInOpened', attempt: typeof attempt === 'number' ? attempt : null })
        } catch (err) { popup?.close(); throw err }
      })
    }}
    onComplete={callbackUrl => act('complete', async () => {
      await post('complete', { callback_url: callbackUrl.trim() })
      popupRef.current?.close()
      dispatch({ type: 'completed' })
      await refresh()
    })}
  />
}
