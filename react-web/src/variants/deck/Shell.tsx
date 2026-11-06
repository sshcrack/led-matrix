import type { ReactNode } from 'react'
import { Link, useLocation, useMatch } from 'react-router-dom'
import { Images, Layers, MoreHorizontal, Radio } from 'lucide-react'
import { cn } from '~/lib/utils'
import { ThemeToggle } from '~/core/ui/ThemeToggle'
import { PowerButton } from '~/core/ui/controls'
import { useNowPlaying } from '~/core/nowPlaying'

const tabs = [
  { to: '/', label: 'Now', icon: Radio, match: (p: string) => p === '/' },
  { to: '/presets', label: 'Presets', icon: Layers, match: (p: string) => p.startsWith('/presets') },
  { to: '/gallery', label: 'Scenes', icon: Images, match: (p: string) => p.startsWith('/gallery') },
  { to: '/more', label: 'More', icon: MoreHorizontal, match: (p: string) => !['/', '/presets', '/gallery'].some(x => x === '/' ? p === '/' : p.startsWith(x)) },
]

export default function DeckShell({ children }: { children: ReactNode }) {
  const { pathname } = useLocation()
  const editing = !!useMatch('/modify-preset/:id')
  const now = useNowPlaying()
  return <div className="min-h-screen bg-background">
    {!editing && <header className="sticky top-0 z-30 flex items-center gap-3 bg-background/85 px-4 py-3 backdrop-blur">
      <Link to="/" className="flex min-w-0 flex-1 items-center gap-2">
        <span className="grid h-9 w-9 shrink-0 place-items-center rounded-2xl bg-primary text-lg text-primary-foreground shadow-md shadow-primary/30">▦</span>
        <span className="min-w-0"><span className="block text-[11px] font-medium uppercase tracking-wider text-muted-foreground">Now playing</span><span className="block truncate text-sm font-bold leading-tight">{now.label}</span></span>
      </Link>
      <PowerButton /><ThemeToggle />
    </header>}
    <main className={cn('mx-auto max-w-3xl px-4', editing ? 'pb-10' : 'pb-32 pt-1')}>{children}</main>
    {!editing && <nav aria-label="Main" className="fixed inset-x-0 bottom-0 z-40 border-t border-border bg-card/90 pb-[env(safe-area-inset-bottom)] backdrop-blur-xl">
      <div className="mx-auto grid max-w-lg grid-cols-4 px-2 py-1.5">
        {tabs.map(t => { const on = t.match(pathname); return <Link key={t.to} to={t.to} aria-current={on ? 'page' : undefined} className={cn('flex flex-col items-center gap-0.5 rounded-2xl py-1.5 text-[11px] font-semibold transition', on ? 'text-primary' : 'text-muted-foreground')}>
          <span className={cn('grid h-8 w-14 place-items-center rounded-full transition', on && 'bg-primary/12')}><t.icon className="h-5 w-5" /></span>{t.label}
        </Link> })}
      </div>
    </nav>}
  </div>
}
