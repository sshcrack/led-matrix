import { useEffect, useState } from 'react'
import { Link, useLocation } from 'react-router-dom'
import { LayoutGrid } from 'lucide-react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'

/** First run of a panel card: points at the Panel hardware card until someone has checked the layout. */
export function PanelSetupBanner() {
  const apiUrl = useApiUrl()
  const { pathname } = useLocation()
  const [needed, setNeeded] = useState(false)
  useEffect(() => {
    if (!apiUrl) return
    const controller = new AbortController()
    fetch(`${apiUrl}/hardware`, { signal: controller.signal })
      .then(res => res.ok ? res.json() : null)
      .then((status: { setup_needed?: boolean } | null) => setNeeded(status?.setup_needed === true))
      .catch(() => {})
    return () => controller.abort()
  }, [apiUrl, pathname])
  if (!needed || pathname === '/integrations') return null
  return <div className="flex items-center gap-3 rounded-lg border border-primary/40 bg-primary/10 px-3 py-2 text-sm">
    <LayoutGrid className="h-4 w-4 shrink-0 text-primary" />
    <span className="min-w-0 flex-1"><span className="font-medium">Set up your panel</span>
      <span className="text-muted-foreground"> · check that the picture is the right way round and the right size.</span></span>
    <Link to="/integrations#panel-hardware" className="flex min-h-10 shrink-0 items-center rounded-md px-3 text-xs font-medium hover:bg-secondary">Set up</Link>
  </div>
}
