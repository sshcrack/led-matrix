import { useEffect, useState } from 'react'
import { WifiOff } from 'lucide-react'
import { useMatrix } from '~/core/matrixContext'

function useBrowserOnline() {
  const [on, setOn] = useState(() => navigator.onLine)
  useEffect(() => {
    const up = () => setOn(true), down = () => setOn(false)
    window.addEventListener('online', up); window.addEventListener('offline', down)
    return () => { window.removeEventListener('online', up); window.removeEventListener('offline', down) }
  }, [])
  return on
}

function ago(ms: number) {
  const s = Math.max(0, Math.round(ms / 1000))
  if (s < 60) return `${s}s ago`
  if (s < 3600) return `${Math.round(s / 60)} min ago`
  return `${Math.round(s / 3600)} h ago`
}

/** Shown when the matrix stops answering /status; tells a phone without network apart from a matrix that is down. */
export function ConnectionBanner() {
  const { online, lastSeen, refreshAll } = useMatrix()
  const browserOnline = useBrowserOnline()
  const [now, setNow] = useState(() => Date.now())
  useEffect(() => { if (online) return; const id = window.setInterval(() => setNow(Date.now()), 5000); return () => window.clearInterval(id) }, [online])
  if (online) return null
  const what = browserOnline ? 'Matrix is offline' : 'This device is offline'
  return <div role="alert" className="flex items-center gap-3 rounded-lg border border-destructive/40 bg-destructive/10 px-3 py-2 text-sm">
    <WifiOff className="h-4 w-4 shrink-0 text-destructive" />
    <span className="min-w-0 flex-1"><span className="font-medium">{what}</span>{lastSeen && <span className="text-muted-foreground"> · last seen {ago(Math.max(now, lastSeen) - lastSeen)}. Shown data may be out of date.</span>}</span>
    <button onClick={refreshAll} className="min-h-10 shrink-0 rounded-md px-3 text-xs font-medium hover:bg-secondary">Retry</button>
  </div>
}
