import { useEffect, useRef, useState, type ReactNode } from 'react'
import { Link, useLocation, useNavigate } from 'react-router-dom'
import { MoreHorizontal, Search } from 'lucide-react'
import { cn } from '~/lib/utils'
import { NAV, isActive } from '~/core/nav'
import { useMatrix } from '~/core/matrixContext'
import { PowerButton } from '~/core/ui/controls'
import { ThemeToggle } from '~/core/ui/ThemeToggle'
import { CommandPalette } from '~/core/ui/CommandPalette'
import { Sheet } from '~/core/ui/Sheet'
import MiniPlayer from './MiniPlayer'
import SpotifyConnection from '~/components/spotify/SpotifyConnection'

const TABS = NAV.filter(n => n.primary)
const MORE = NAV.filter(n => !n.primary)

/** Desktop: top bar with nav + ⌘K. Phone: compact top bar, mini-player and a bottom tab bar (thumb reach). */
export default function ConsoleShell({ children }: { children: ReactNode }) {
  const { pathname } = useLocation()
  // Other pages (Scene Lab, providers, …) write presets/looks through their own calls; the shared
  // lists live as long as the app, so re-sync them whenever the user navigates.
  const { refreshAll } = useMatrix()
  const refreshRef = useRef(refreshAll)
  useEffect(() => { refreshRef.current = refreshAll })
  const firstPath = useRef(true)
  useEffect(() => { if (firstPath.current) { firstPath.current = false; return } refreshRef.current() }, [pathname])
  const navigate = useNavigate()
  const [palette, setPalette] = useState(false)
  const [more, setMore] = useState(false)
  const [lastPath, setLastPath] = useState(pathname)
  if (lastPath !== pathname) { setLastPath(pathname); setMore(false) }
  const moreActive = MORE.some(n => isActive(n, pathname))

  return <div className="min-h-screen bg-background">
    <header className="sticky top-0 z-30 border-b border-border bg-background/85 pt-[env(safe-area-inset-top)] backdrop-blur">
      <div className="mx-auto flex h-14 max-w-[96rem] items-center gap-3 px-3 sm:px-4 lg:h-12">
        <Link to="/" className="flex items-center gap-2 font-mono text-sm font-bold tracking-tight"><span className="grid h-7 w-7 place-items-center rounded bg-primary text-xs text-primary-foreground">▦</span><span>matrix</span></Link>
        <nav className="hidden min-w-0 flex-1 gap-0.5 lg:flex" aria-label="Main">
          {NAV.map(n => <Link key={n.to} to={n.to} className={cn('shrink-0 rounded-md px-2.5 py-1.5 text-xs font-medium transition', isActive(n, pathname) ? 'bg-secondary text-foreground' : 'text-muted-foreground hover:text-foreground')}>{n.label}</Link>)}
        </nav>
        <span className="flex-1 lg:hidden" />
        <button onClick={() => setPalette(true)} className="hidden items-center gap-2 rounded-md border border-border bg-card px-2.5 py-1.5 text-xs text-muted-foreground hover:text-foreground lg:flex">
          <Search className="h-3.5 w-3.5" />Search<kbd className="rounded bg-secondary px-1.5 font-mono text-[10px]">⌘K</kbd>
        </button>
        <button onClick={() => setPalette(true)} aria-label="Search" className="grid h-10 w-10 place-items-center rounded-full text-muted-foreground active:bg-secondary lg:hidden"><Search className="h-5 w-5" /></button>
        <PowerButton /><ThemeToggle className="hidden lg:grid" />
      </div>
    </header>

    <main className="mx-auto max-w-[96rem] p-3 pb-44 sm:p-4 sm:pb-44 lg:pb-4"><div className="space-y-4">{pathname !== '/integrations' && <SpotifyConnection />}{children}</div>
    </main>

    {/* Phone chrome */}
    <div className="fixed inset-x-0 bottom-0 z-40 lg:hidden">
      <div className="px-2 pb-2"><MiniPlayer /></div>
      <nav aria-label="Main" className="border-t border-border bg-card/95 pb-[env(safe-area-inset-bottom)] backdrop-blur-xl">
        <div className="mx-auto grid max-w-md grid-cols-4">
          {TABS.map(n => { const on = isActive(n, pathname); return <Link key={n.to} to={n.to} aria-current={on ? 'page' : undefined} className={cn('flex min-h-14 flex-col items-center justify-center gap-0.5 text-[11px] font-medium transition active:bg-secondary', on ? 'text-primary' : 'text-muted-foreground')}>{n.icon}{n.label}</Link> })}
          <button onClick={() => setMore(true)} aria-label="More" className={cn('flex min-h-14 flex-col items-center justify-center gap-0.5 text-[11px] font-medium active:bg-secondary', moreActive ? 'text-primary' : 'text-muted-foreground')}><MoreHorizontal className="h-5 w-5" />More</button>
        </div>
      </nav>
    </div>

    <Sheet open={more} onOpenChange={setMore} title="More">
      <ul className="space-y-1">
        {MORE.map(n => <li key={n.to}><Link to={n.to} className="flex items-center gap-3 rounded-xl px-3 py-3 active:bg-secondary"><span className="text-primary">{n.icon}</span><span><span className="block font-medium">{n.label}</span><span className="block text-xs text-muted-foreground">{n.description}</span></span></Link></li>)}
      </ul>
      <div className="mt-3 flex items-center justify-between rounded-xl border border-border px-3 py-2"><span className="text-sm">Theme</span><ThemeToggle /></div>
    </Sheet>

    <CommandPalette open={palette} onOpenChange={setPalette} onNewPreset={() => navigate('/?new=1')} onEditPreset={id => navigate(`/?edit=${encodeURIComponent(id)}`)} />
  </div>
}
