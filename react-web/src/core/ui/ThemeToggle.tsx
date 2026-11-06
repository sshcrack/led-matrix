import { Moon, Sun } from 'lucide-react'
import { useState } from 'react'
import { cn } from '~/lib/utils'

export function ThemeToggle({ className }: { className?: string }) {
  const [dark, setDark] = useState(() => document.documentElement.classList.contains('dark'))
  const toggle = () => {
    document.documentElement.classList.toggle('dark', !dark)
    try { localStorage.setItem('colorScheme', dark ? 'light' : 'dark') } catch { /* ignore */ }
    setDark(!dark)
  }
  return <button onClick={toggle} aria-label="Toggle theme" className={cn('grid h-9 w-9 place-items-center rounded-full text-muted-foreground transition hover:bg-secondary hover:text-foreground', className)}>
    {dark ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
  </button>
}
