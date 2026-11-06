import { useState, type ReactNode } from 'react'
import { Link, useLocation, useMatch, useNavigate } from 'react-router-dom'
import { Menu, Pencil } from 'lucide-react'
import { cn } from '~/lib/utils'
import { NAV, isActive } from '~/core/nav'
import { useMatrix } from '~/core/matrixContext'
import { Sheet } from '~/core/ui/Sheet'
import { ThemeToggle } from '~/core/ui/ThemeToggle'
import { PowerButton } from '~/core/ui/controls'
import { PresetMenu } from '~/core/ui/PresetMenu'

export default function AmbientShell({ children }: { children: ReactNode }) {
  const { pathname } = useLocation()
  const navigate = useNavigate()
  const home = !!useMatch('/')
  const { presets } = useMatrix()
  const [menu, setMenu] = useState(false)
  const [lastPath, setLastPath] = useState(pathname)
  if (lastPath !== pathname) { setLastPath(pathname); setMenu(false) }
  return <div className="relative min-h-screen overflow-x-hidden bg-background">
    <div aria-hidden className="pointer-events-none fixed inset-0 -z-0 opacity-80 [background:radial-gradient(60rem_40rem_at_50%_-10%,rgb(var(--primary)/0.18),transparent_70%),radial-gradient(40rem_30rem_at_90%_100%,rgb(var(--primary)/0.10),transparent_70%)]" />
    <header className={cn('relative z-20 flex items-center justify-between px-4 py-3 sm:px-6', home && 'absolute inset-x-0 top-0')}>
      <button onClick={() => setMenu(true)} aria-label="Menu" className="grid h-10 w-10 place-items-center rounded-full bg-card/60 ring-1 ring-border backdrop-blur hover:bg-card"><Menu className="h-5 w-5" /></button>
      <div className="flex items-center gap-1.5"><ThemeToggle className="bg-card/60 ring-1 ring-border backdrop-blur" /><PowerButton /></div>
    </header>
    <main className="relative z-10">{!home ? <div className="mx-auto max-w-6xl px-4 pb-10 sm:px-6">{children}</div> : children}</main>

    <Sheet side="left" open={menu} onOpenChange={setMenu} title="Matrix" description="Everything else lives here.">
      <nav className="space-y-1" aria-label="Main">
        {NAV.map(n => <Link key={n.to} to={n.to} className={cn('flex items-center gap-3 rounded-xl px-3 py-2.5 text-sm font-medium', isActive(n, pathname) ? 'bg-primary/12 text-primary' : 'text-muted-foreground hover:bg-secondary')}>{n.icon}<span>{n.label}<span className="block text-[11px] font-normal opacity-70">{n.description}</span></span></Link>)}
      </nav>
      <h3 className="mb-1 mt-6 px-3 text-[11px] font-semibold uppercase tracking-widest text-muted-foreground">Your moods</h3>
      <ul>{Object.entries(presets ?? {}).map(([id, p]) => <li key={id} className="flex items-center rounded-xl pl-3 hover:bg-secondary">
        <span className="flex-1 truncate py-2 text-sm">{p.display_name || id}</span>
        <button onClick={() => navigate(`/modify-preset/${encodeURIComponent(id)}`)} aria-label={`Edit ${p.display_name || id}`} className="grid h-9 w-9 place-items-center text-muted-foreground"><Pencil className="h-4 w-4" /></button>
        <PresetMenu id={id} className="h-9 w-9" /></li>)}</ul>
    </Sheet>
  </div>
}
