import { Link, useLocation, useMatch } from 'react-router-dom'
import { Grid3x3, Calendar, Download, Moon, Sun, Images, FolderCode, Menu, X, Sparkles, Activity } from 'lucide-react'
import { cn } from '~/lib/utils'
import { useState, useEffect } from 'react'

interface NavItem { to: string; label: string; description: string; icon: React.ReactNode }
const navItems: NavItem[] = [
  { to: '/', label: 'Control', description: 'Power and presets', icon: <Grid3x3 className="h-5 w-5" /> },
  { to: '/gallery', label: 'Scenes', description: 'Browse and preview', icon: <Images className="h-5 w-5" /> },
  { to: '/schedules', label: 'Automation', description: 'Schedules', icon: <Calendar className="h-5 w-5" /> },
  { to: '/assets', label: 'Assets', description: 'Shaders and media', icon: <FolderCode className="h-5 w-5" /> },
  { to: '/updates', label: 'Updates', description: 'System updates', icon: <Download className="h-5 w-5" /> },
  { to: '/diagnostics', label: 'Health', description: 'Live diagnostics', icon: <Activity className="h-5 w-5" /> },
]

function ThemeToggle() {
  const [isDark, setIsDark] = useState(() => document.documentElement.classList.contains('dark'))
  const toggle = () => {
    document.documentElement.classList.toggle('dark', !isDark)
    localStorage.setItem('colorScheme', isDark ? 'light' : 'dark')
    setIsDark(!isDark)
  }
  return <button onClick={toggle} className="rounded-xl border border-border bg-background/60 p-2.5 transition hover:bg-secondary" aria-label="Toggle theme">{isDark ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}</button>
}

export default function Layout({ children }: { children: React.ReactNode }) {
  const location = useLocation()
  const [mobileOpen, setMobileOpen] = useState(false)
  useEffect(() => {
    const stored = localStorage.getItem('colorScheme')
    const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches
    document.documentElement.classList.toggle('dark', stored === 'dark' || (!stored && prefersDark))
  }, [])
  useEffect(() => setMobileOpen(false), [location.pathname])

  if (useMatch('/modify-:id/*')) return <main className="min-h-screen app-surface">{children}</main>

  const nav = (compact = false) => navItems.map(item => {
    const active = item.to === '/' ? location.pathname === '/' : location.pathname.startsWith(item.to)
    return <Link key={item.to} to={item.to} className={cn('group flex items-center gap-3 rounded-xl transition', compact ? 'px-3 py-3' : 'px-3 py-2.5', active ? 'bg-primary text-primary-foreground shadow-lg shadow-primary/20' : 'text-muted-foreground hover:bg-secondary hover:text-foreground')}>
      <span className={cn('grid h-9 w-9 shrink-0 place-items-center rounded-lg', active ? 'bg-white/15' : 'bg-secondary group-hover:bg-background')}>{item.icon}</span>
      <span className="min-w-0"><span className="block text-sm font-semibold">{item.label}</span>{!compact && <span className={cn('block text-[11px]', active ? 'text-primary-foreground/65' : 'text-muted-foreground')}>{item.description}</span>}</span>
    </Link>
  })

  return <div className="min-h-screen app-surface">
    <aside className="glass-panel fixed inset-y-3 left-3 z-30 hidden w-64 flex-col rounded-2xl lg:flex">
      <div className="flex items-center gap-3 p-4">
        <div className="grid h-11 w-11 place-items-center rounded-xl bg-gradient-to-br from-primary to-sky-400 text-white shadow-lg shadow-primary/25"><Sparkles className="h-5 w-5" /></div>
        <div><div className="font-bold tracking-tight">Matrix Studio</div><div className="text-xs text-muted-foreground">128 × 128 controller</div></div>
      </div>
      <nav className="flex-1 space-y-1.5 p-3">{nav()}</nav>
      <div className="flex items-center justify-between border-t border-border p-3"><span className="px-2 text-xs text-muted-foreground">LED Matrix</span><ThemeToggle /></div>
    </aside>

    <header className="glass-panel fixed left-3 right-3 top-3 z-30 flex h-14 items-center justify-between rounded-2xl px-3 lg:hidden">
      <button onClick={() => setMobileOpen(true)} className="rounded-xl p-2 hover:bg-secondary"><Menu className="h-5 w-5" /></button>
      <div className="flex items-center gap-2 font-semibold"><Grid3x3 className="h-4 w-4 text-primary" /> Matrix Studio</div>
      <ThemeToggle />
    </header>

    {mobileOpen && <div className="fixed inset-0 z-50 lg:hidden">
      <button className="absolute inset-0 bg-black/55 backdrop-blur-sm" onClick={() => setMobileOpen(false)} aria-label="Close navigation" />
      <aside className="absolute inset-y-0 left-0 w-[min(88vw,340px)] bg-card p-3 shadow-2xl">
        <div className="mb-4 flex items-center justify-between p-2"><div className="font-bold">Matrix Studio</div><button onClick={() => setMobileOpen(false)} className="rounded-lg p-2 hover:bg-secondary"><X className="h-5 w-5" /></button></div>
        <nav className="space-y-2">{nav(true)}</nav>
      </aside>
    </div>}

    <main className="min-h-screen pt-20 lg:ml-[276px] lg:pt-0">
      <div className="mx-auto max-w-7xl p-4 sm:p-6 lg:p-8">{children}</div>
    </main>

    <nav className="glass-panel fixed bottom-3 left-3 right-3 z-30 grid grid-cols-6 rounded-2xl p-1.5 lg:hidden">
      {navItems.map(item => {
        const active = item.to === '/' ? location.pathname === '/' : location.pathname.startsWith(item.to)
        return <Link key={item.to} to={item.to} className={cn('flex min-w-0 flex-col items-center gap-1 rounded-xl px-1 py-2 text-[10px] font-medium', active ? 'bg-primary text-primary-foreground' : 'text-muted-foreground')}><span>{item.icon}</span><span className="truncate">{item.label}</span></Link>
      })}
    </nav>
  </div>
}
