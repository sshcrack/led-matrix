import { useState, type ReactNode } from 'react'
import { Link, useLocation, useNavigate } from 'react-router-dom'
import { Search } from 'lucide-react'
import { cn } from '~/lib/utils'
import { NAV, isActive } from '~/core/nav'
import { PowerButton } from '~/core/ui/controls'
import { ThemeToggle } from '~/core/ui/ThemeToggle'
import { CommandPalette } from '~/core/ui/CommandPalette'

export default function ConsoleShell({ children }: { children: ReactNode }) {
  const { pathname } = useLocation()
  const navigate = useNavigate()
  const [palette, setPalette] = useState(false)
  return <div className="min-h-screen bg-background">
    <header className="sticky top-0 z-30 border-b border-border bg-background/85 backdrop-blur">
      <div className="mx-auto flex h-12 max-w-[96rem] items-center gap-3 px-3 sm:px-4">
        <Link to="/" className="flex items-center gap-2 font-mono text-sm font-bold tracking-tight"><span className="grid h-6 w-6 place-items-center rounded bg-primary text-[11px] text-primary-foreground">▦</span><span className="hidden sm:inline">matrix</span></Link>
        <nav className="flex min-w-0 flex-1 gap-0.5 overflow-x-auto" aria-label="Main">
          {NAV.map(n => <Link key={n.to} to={n.to} className={cn('shrink-0 rounded-md px-2.5 py-1.5 text-xs font-medium transition', isActive(n, pathname) ? 'bg-secondary text-foreground' : 'text-muted-foreground hover:text-foreground')}>{n.label}</Link>)}
        </nav>
        <button onClick={() => setPalette(true)} className="hidden items-center gap-2 rounded-md border border-border bg-card px-2.5 py-1.5 text-xs text-muted-foreground hover:text-foreground sm:flex">
          <Search className="h-3.5 w-3.5" />Search<kbd className="rounded bg-secondary px-1.5 font-mono text-[10px]">⌘K</kbd>
        </button>
        <button onClick={() => setPalette(true)} aria-label="Search" className="grid h-9 w-9 place-items-center rounded-full text-muted-foreground hover:bg-secondary sm:hidden"><Search className="h-4 w-4" /></button>
        <PowerButton /><ThemeToggle />
      </div>
    </header>
    <main className="mx-auto max-w-[96rem] p-3 sm:p-4">{children}</main>
    <CommandPalette open={palette} onOpenChange={setPalette} onNewPreset={() => navigate('/?new=1')} onEditPreset={id => navigate(`/?edit=${encodeURIComponent(id)}`)} />
  </div>
}
