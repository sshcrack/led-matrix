import { useCallback, useEffect, useRef, useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import SpotifySetup, { type SpotifySetupStatus } from './SpotifySetup'

/** The plugin owns setup; older daemons and installs without it return 404. */
export default function SpotifyConnection() {
  const apiUrl = useApiUrl()
  const [status, setStatus] = useState<SpotifySetupStatus | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [notice, setNotice] = useState<string | null>(null)
  const [connecting, setConnecting] = useState(false)
  const popupRef = useRef<Window | null>(null)

  const refresh = useCallback(async () => {
    if (!apiUrl) return
    try {
      const res = await fetch(`${apiUrl}/spotify/status`)
      if (res.status === 404) { setStatus(null); return }
      if (!res.ok) throw new Error('Could not check Spotify connection. Try again.')
      const next = await res.json() as SpotifySetupStatus
      setStatus(next)
      if (next.connected) setConnecting(false)
    } catch (err) { setError(err instanceof Error ? err.message : 'Could not check Spotify connection.') }
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
  useEffect(() => {
    if (!connecting) return
    const timer = window.setInterval(() => { if (!document.hidden) void refresh() }, 2000)
    const expiry = window.setTimeout(() => setConnecting(false), 10 * 60 * 1000)
    return () => { window.clearInterval(timer); window.clearTimeout(expiry) }
  }, [connecting, refresh])

  async function post(path: string, body: Record<string, string> = {}) {
    const res = await fetch(`${apiUrl}/spotify/${path}`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
    })
    const data = await res.json() as { error?: string; url?: string }
    if (!res.ok) throw new Error(data.error || 'Spotify setup failed. Try again.')
    return data
  }

  async function act(fn: () => Promise<void>) {
    setBusy(true); setError(null); setNotice(null)
    try { await fn() }
    catch (err) {
      setError(err instanceof Error ? err.message : 'Spotify setup failed. Try again.')
      throw err
    } finally { setBusy(false) }
  }

  if (!status) return null
  return <SpotifySetup status={status} busy={busy} error={error} notice={notice}
    onRefresh={() => { void refresh() }}
    onSave={(clientId, clientSecret) => act(async () => {
      await post('credentials', { client_id: clientId.trim(), client_secret: clientSecret.trim() })
      setConnecting(false)
      await refresh()
      setNotice('App credentials saved. Connect your Spotify account next.')
    })}
    onConnect={() => {
      // Open during the click so mobile popup blockers permit the sign-in tab.
      const popup = window.open('about:blank', '_blank')
      if (popup) popup.opener = null
      popupRef.current = popup
      return act(async () => {
        try {
          const { url } = await post('connect')
          if (!url || !url.startsWith('https://accounts.spotify.com/authorize?')) throw new Error('Spotify did not return a sign-in address.')
          if (!popup) throw new Error('Allow pop-ups for this page, then choose Connect Spotify again.')
          popup.location.href = url
          setConnecting(true)
          setNotice('Finish signing in in the Spotify tab. If the returned page cannot load, copy its full address and paste it below.')
        } catch (err) { popup?.close(); throw err }
      })
    }}
    onComplete={callbackUrl => act(async () => {
      await post('complete', { callback_url: callbackUrl.trim() })
      popupRef.current?.close()
      setConnecting(false)
      await refresh()
      setNotice('Spotify connected. Play music in Spotify to bring it to your matrix.')
    })}
  />
}
