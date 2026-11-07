import { useEffect, useId, useRef, useState, type FormEvent, type ReactNode } from 'react'
import { Check, ChevronDown, CircleAlert, Copy, ExternalLink, Info, Loader2, Music2, RefreshCw } from 'lucide-react'

export interface SpotifySetupStatus {
  configured: boolean
  connected: boolean
  client_id: string
  redirect_uri: string
}

export interface SpotifySetupProps {
  status: SpotifySetupStatus
  busy: boolean
  error: string | null
  notice: string | null
  onSave: (clientId: string) => Promise<void>
  onConnect: () => Promise<void>
  onComplete: (callbackUrl: string) => Promise<void>
  onRefresh: () => void
  onDismiss?: () => void
  initiallyOpen?: boolean
}

type Action = 'save' | 'connect' | 'complete' | null

const DASHBOARD_URL = 'https://developer.spotify.com/dashboard'

// Shared touch-friendly styling (>=40px tall, inputs 16px so iOS does not zoom).
const focusRing = 'focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 focus-visible:ring-offset-background'
const btnBase = `inline-flex min-h-10 items-center justify-center gap-2 rounded-lg px-4 text-sm font-medium transition-colors disabled:pointer-events-none disabled:opacity-50 ${focusRing}`
const btnPrimary = `${btnBase} bg-primary text-primary-foreground hover:bg-primary/90`
const btnOutline = `${btnBase} border border-border bg-background hover:bg-secondary hover:text-secondary-foreground`
const btnGhost = `${btnBase} text-muted-foreground hover:bg-secondary hover:text-foreground`
const inputCls = 'block h-11 w-full rounded-lg border border-input bg-background px-3 text-base shadow-sm transition-colors placeholder:text-muted-foreground focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring disabled:cursor-not-allowed disabled:opacity-50'

function Step({ n, title, done, children }: { n: number; title: string; done?: boolean; children: ReactNode }) {
  return (
    <li className="flex gap-3">
      <span
        aria-hidden
        className={`mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full text-xs font-semibold ${done ? 'bg-success text-success-foreground' : 'bg-secondary text-secondary-foreground'}`}
      >
        {done ? <Check className="h-3.5 w-3.5" /> : n}
      </span>
      <div className="min-w-0 flex-1 space-y-2.5">
        <h3 className="text-sm font-semibold leading-6">{title}</h3>
        {children}
      </div>
    </li>
  )
}

function Spinner() {
  return <Loader2 className="h-4 w-4 animate-spin" aria-hidden />
}

export default function SpotifySetup({ status, busy, error, notice, onSave, onConnect, onComplete, onRefresh, onDismiss, initiallyOpen }: SpotifySetupProps) {
  const { configured, connected, redirect_uri: redirectUri } = status
  const uid = useId()
  const panelId = `${uid}-panel`

  // Fresh installs open the guide; everything else starts as a compact row.
  const [open, setOpen] = useState(initiallyOpen ?? (!configured && !connected))
  const [clientId, setClientId] = useState(status.client_id)
  const [callbackUrl, setCallbackUrl] = useState('')
  const [copied, setCopied] = useState(false)
  const [action, setAction] = useState<Action>(null)
  const [showCallback, setShowCallback] = useState(false)
  const copyTimer = useRef<number | undefined>(undefined)

  useEffect(() => () => window.clearTimeout(copyTimer.current), [])

  const disabled = busy || action !== null
  const credsDirty = clientId.trim() !== status.client_id
  const canSave = clientId.trim() !== ''

  async function run(kind: Exclude<Action, null>, fn: () => Promise<void>): Promise<boolean> {
    setAction(kind)
    try {
      await fn()
      return true
    } catch {
      return false // the parent reports failures through `error`
    } finally {
      setAction(null)
    }
  }

  async function copyRedirect() {
    try {
      await navigator.clipboard.writeText(redirectUri)
    } catch {
      // Clipboard API needs a secure context; the Pi is usually plain http, so fall back to a selection copy.
      const ta = document.createElement('textarea')
      ta.value = redirectUri
      ta.setAttribute('readonly', '')
      ta.style.position = 'fixed'
      ta.style.opacity = '0'
      document.body.appendChild(ta)
      ta.select()
      try { document.execCommand('copy') } catch { /* nothing else to try */ }
      document.body.removeChild(ta)
    }
    setCopied(true)
    window.clearTimeout(copyTimer.current)
    copyTimer.current = window.setTimeout(() => setCopied(false), 2000)
  }

  async function submitCreds(e: FormEvent) {
    e.preventDefault()
    if (!canSave || disabled) return
    await run('save', () => onSave(clientId.trim()))
  }

  async function submitCallback(e: FormEvent) {
    e.preventDefault()
    if (!callbackUrl.trim() || disabled) return
    if (await run('complete', () => onComplete(callbackUrl.trim()))) setCallbackUrl('')
  }

  const tone = connected ? 'border-border bg-card' : 'border-primary/30 bg-primary/5'

  return (
    <section aria-label="Spotify connection" className={`rounded-2xl border ${tone} text-card-foreground`}>
      <div className="flex items-center gap-3 p-3 sm:px-4">
        <span className={`flex h-10 w-10 shrink-0 items-center justify-center rounded-full ${connected ? 'bg-success/15 text-success' : 'bg-primary/15 text-primary'}`}>
          <Music2 className="h-5 w-5" aria-hidden />
        </span>
        <div className="min-w-0 flex-1">
          <p className="text-sm font-semibold leading-5">{connected ? 'Spotify connected' : configured ? 'Spotify is not connected' : 'Connect Spotify'}</p>
          <p className="truncate text-xs text-muted-foreground">
            {connected
              ? 'Now-playing scenes are available.'
              : configured
                ? 'Client ID saved. Finish by connecting your account.'
                : 'Show album art and what is playing on the matrix.'}
          </p>
        </div>
        <button
          type="button"
          className={`${connected ? btnGhost : btnPrimary} shrink-0`}
          aria-expanded={open}
          aria-controls={panelId}
          onClick={() => setOpen(o => !o)}
        >
          <span>{connected ? 'Settings' : open ? 'Hide setup' : configured ? 'Continue setup' : 'Set up'}</span>
          <ChevronDown className={`h-4 w-4 transition-transform ${open ? 'rotate-180' : ''}`} aria-hidden />
        </button>
      </div>

      {onDismiss && <div className="flex flex-wrap items-center gap-x-3 border-t border-border px-3 py-2 sm:px-4">
        <button type="button" onClick={onDismiss} disabled={disabled} className={btnGhost}>Dismiss setup</button>
        <p className="text-xs text-muted-foreground">Open it again in Integrations whenever you’re ready.</p>
      </div>}

      {/* Feedback stays visible even when the guide is collapsed. */}
      {(error || notice) && (
        <div className="space-y-2 px-3 pb-3 sm:px-4">
          {error && (
            <p role="alert" className="flex items-start gap-2 rounded-lg bg-destructive/10 px-3 py-2 text-sm text-destructive">
              <CircleAlert className="mt-0.5 h-4 w-4 shrink-0" aria-hidden />
              <span className="min-w-0 break-words">{error}</span>
            </p>
          )}
          {notice && (
            <p role="status" className="flex items-start gap-2 rounded-lg bg-success/10 px-3 py-2 text-sm text-foreground">
              <Check className="mt-0.5 h-4 w-4 shrink-0 text-success" aria-hidden />
              <span className="min-w-0 break-words">{notice}</span>
            </p>
          )}
        </div>
      )}

      {open && (
        <div id={panelId} className="border-t border-border/70 p-3 sm:p-4">
          <ol className="space-y-5">
            <Step n={1} title="Create a Spotify app" done={configured}>
              <p className="text-sm text-muted-foreground">
                Create an app in the Spotify developer dashboard and add this exact redirect URI to it. It is a small page that sends your browser back to your matrix, so it works from any device on your network.
              </p>
              <div className="flex flex-col gap-2 sm:flex-row sm:items-stretch">
                <code className="block min-h-10 flex-1 select-all break-all rounded-lg border border-input bg-background px-3 py-2 text-sm leading-6">
                  {redirectUri}
                </code>
                <button type="button" className={btnOutline} onClick={() => void copyRedirect()} aria-label="Copy redirect URI">
                  {copied ? <Check className="h-4 w-4 text-success" aria-hidden /> : <Copy className="h-4 w-4" aria-hidden />}
                  <span aria-live="polite">{copied ? 'Copied' : 'Copy'}</span>
                </button>
              </div>
              <a href={DASHBOARD_URL} target="_blank" rel="noreferrer" className={`${btnOutline} w-full sm:w-auto`}>
                Open Spotify dashboard
                <ExternalLink className="h-4 w-4" aria-hidden />
              </a>
            </Step>

            <li className="pl-9 text-sm text-muted-foreground">
              Spotify requires the app owner to have Premium in development mode. Sign in with the app owner’s account, or add your account in the app’s User Management settings.
            </li>
            <Step n={2} title="Paste its Client ID" done={configured}>
              <form onSubmit={e => void submitCreds(e)} className="space-y-3">
                <div className="space-y-1.5">
                  <label htmlFor={`${uid}-id`} className="text-sm font-medium">Client ID</label>
                  <input
                    id={`${uid}-id`}
                    className={inputCls}
                    value={clientId}
                    onChange={e => setClientId(e.target.value)}
                    autoComplete="off"
                    autoCapitalize="off"
                    autoCorrect="off"
                    spellCheck={false}
                    disabled={disabled}
                  />
                </div>
                <button type="submit" className={`${configured && !credsDirty ? btnOutline : btnPrimary} w-full sm:w-auto`} disabled={disabled || !canSave}>
                  {action === 'save' ? <Spinner /> : null}
                  Save Client ID
                </button>
              </form>
            </Step>

            <Step n={3} title="Connect your account" done={connected}>
              <p className="text-sm text-muted-foreground">
                {connected ? 'Connect again to switch Spotify accounts or renew access.' : 'Approve access in the Spotify tab. You will be sent back to this matrix automatically, from any phone or computer on your network.'}
              </p>
              <div className="flex flex-col gap-2 sm:flex-row">
                <button
                  type="button"
                  className={`${configured ? btnPrimary : btnOutline} w-full sm:w-auto`}
                  disabled={disabled || !configured}
                  onClick={() => { setShowCallback(true); void run('connect', onConnect) }}
                >
                  {action === 'connect' ? <Spinner /> : null}
                  {connected ? 'Reconnect Spotify' : 'Connect Spotify'}
                </button>
                <button type="button" className={`${btnGhost} w-full sm:w-auto`} onClick={onRefresh} disabled={busy}>
                  <RefreshCw className={`h-4 w-4 ${busy ? 'animate-spin' : ''}`} aria-hidden />
                  Check status
                </button>
              </div>
              {!configured && <p className="text-xs text-muted-foreground">Save your Client ID first.</p>}
            </Step>
          </ol>

          {configured && (
            <div className="mt-5 rounded-xl bg-secondary/60">
              <button
                type="button"
                className={`flex min-h-10 w-full items-center gap-2 rounded-xl px-3 py-2 text-left text-sm font-medium ${focusRing}`}
                aria-expanded={showCallback}
                aria-controls={`${uid}-callback`}
                onClick={() => setShowCallback(s => !s)}
              >
                <Info className="h-4 w-4 shrink-0 text-muted-foreground" aria-hidden />
                <span className="flex-1">Not sent back to your matrix?</span>
                <ChevronDown className={`h-4 w-4 shrink-0 transition-transform ${showCallback ? 'rotate-180' : ''}`} aria-hidden />
              </button>
              {showCallback && (
                <form id={`${uid}-callback`} onSubmit={e => void submitCallback(e)} className="space-y-3 px-3 pb-3">
                  <p className="text-sm text-muted-foreground">
                    If the page after Spotify does not return you to your matrix, copy its full address from the browser bar and paste it here to finish.
                  </p>
                  <div className="space-y-1.5">
                    <label htmlFor={`${uid}-cb`} className="text-sm font-medium">Address of the page that failed</label>
                    <input
                      id={`${uid}-cb`}
                      className={inputCls}
                      type="url"
                      inputMode="url"
                      value={callbackUrl}
                      onChange={e => setCallbackUrl(e.target.value)}
                      placeholder={`${redirectUri}?code=…`}
                      autoComplete="off"
                      autoCapitalize="off"
                      autoCorrect="off"
                      spellCheck={false}
                      disabled={disabled}
                    />
                  </div>
                  <button type="submit" className={`${btnPrimary} w-full sm:w-auto`} disabled={disabled || !callbackUrl.trim()}>
                    {action === 'complete' ? <Spinner /> : null}
                    Finish connecting
                  </button>
                </form>
              )}
            </div>
          )}
        </div>
      )}
    </section>
  )
}
