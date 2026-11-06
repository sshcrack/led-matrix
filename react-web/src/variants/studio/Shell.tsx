import type { ReactNode } from 'react'
import { Link, useLocation } from 'react-router-dom'
import { cn } from '~/lib/utils'
import { NAV, isActive } from '~/core/nav'
import { ThemeToggle } from '~/core/ui/ThemeToggle'
import { ModeSwitch, PowerButton } from '~/core/ui/controls'
import { SceneThumb } from '~/core/ui/SceneThumb'
import { useNowPlaying } from '~/core/nowPlaying'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '~/components/ui/tooltip'

export default function StudioShell({ children }: { children: ReactNode }) {
  const { pathname } = useLocation()
  const now = useNowPlaying()
  return <TooltipProvider delayDuration={150}><div className="min-h-screen bg-background">
    {/* Rail (desktop) */}
    <aside className="fixed inset-y-0 left-0 z-30 hidden w-[4.5rem] flex-col items-center gap-1 border-r border-border bg-card py-4 lg:flex">
      <Link to="/" className="mb-3 grid h-10 w-10 place-items-center rounded-xl bg-primary text-primary-foreground" aria-label="Home">▦</Link>
      {NAV.map(n => <Tooltip key={n.to}><TooltipTrigger asChild>
        <Link to={n.to} aria-label={n.label} aria-current={isActive(n, pathname) ? 'page' : undefined} className={cn('flex w-14 flex-col items-center gap-0.5 rounded-lg py-2 text-[10px] font-medium transition', isActive(n, pathname) ? 'bg-primary/10 text-primary' : 'text-muted-foreground hover:bg-secondary hover:text-foreground')}>{n.icon}{n.label}</Link>
      </TooltipTrigger><TooltipContent side="right">{n.description}</TooltipContent></Tooltip>)}
      <div className="mt-auto"><ThemeToggle /></div>
    </aside>
    {/* Top strip (mobile) */}
    <header className="sticky top-0 z-30 flex items-center gap-1 overflow-x-auto border-b border-border bg-card/95 px-3 py-2 backdrop-blur lg:hidden">
      {NAV.map(n => <Link key={n.to} to={n.to} className={cn('shrink-0 rounded-lg px-3 py-1.5 text-sm font-medium', isActive(n, pathname) ? 'bg-primary/10 text-primary' : 'text-muted-foreground')}>{n.label}</Link>)}
    </header>

    <main className="px-4 pb-28 pt-6 sm:px-8 lg:pl-[calc(4.5rem+2rem)]"><div className="mx-auto max-w-6xl">{children}</div></main>

    {/* Docked player */}
    <div className="fixed inset-x-0 bottom-0 z-40 border-t border-border bg-card/95 backdrop-blur-xl lg:pl-[4.5rem]">
      <div className="mx-auto flex max-w-6xl items-center gap-3 px-4 py-2.5 sm:px-8">
        <SceneThumb name={now.firstScene ?? ''} className="h-11 w-11 shrink-0 rounded-lg" />
        <div className="min-w-0 flex-1"><div className="text-[10px] font-semibold uppercase tracking-wider text-muted-foreground">{now.off ? 'Display off' : 'Now playing'}</div><div className="truncate text-sm font-semibold">{now.label}</div></div>
        <ModeSwitch className="hidden sm:inline-flex" /><PowerButton />
      </div>
    </div>
  </div></TooltipProvider>
}
