import { Link, useLocation } from 'react-router-dom'
import { Grid3x3, Calendar, Download, Moon, Sun, Images, FolderCode } from 'lucide-react'
import { cn } from '~/lib/utils'
import { useState, useEffect } from 'react'

interface NavItem {
  to: string
  label: string
  icon: React.ReactNode
}

const navItems: NavItem[] = [
  { to: '/', label: 'Home', icon: <Grid3x3 className="h-5 w-5" /> },
  { to: '/gallery', label: 'Gallery', icon: <Images className="h-5 w-5" /> },
  { to: '/assets', label: 'Assets', icon: <FolderCode className="h-5 w-5" /> },
  { to: '/schedules', label: 'Schedules', icon: <Calendar className="h-5 w-5" /> },
  { to: '/updates', label: 'Updates', icon: <Download className="h-5 w-5" /> },
]

function ThemeToggle() {
  const [isDark, setIsDark] = useState(() =>
    document.documentElement.classList.contains('dark')
  )

  const toggle = () => {
    const root = document.documentElement
    if (isDark) {
      root.classList.remove('dark')
      localStorage.setItem('colorScheme', 'light')
      setIsDark(false)
    } else {
      root.classList.add('dark')
      localStorage.setItem('colorScheme', 'dark')
      setIsDark(true)
    }
  }

  return (
    <button
      onClick={toggle}
      className="p-2 rounded-lg hover:bg-secondary transition-colors"
      aria-label="Toggle theme"
    >
      {isDark ? <Sun className="h-5 w-5" /> : <Moon className="h-5 w-5" />}
    </button>
  )
}

export default function Layout({ children }: { children: React.ReactNode }) {
  const location = useLocation()

  // Initialize theme
  useEffect(() => {
    const stored = localStorage.getItem('colorScheme')
    const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches
    if (stored === 'dark' || (!stored && prefersDark)) {
      document.documentElement.classList.add('dark')
    }
  }, [])

  const isModifyPage = location.pathname.startsWith('/modify-')

  if (isModifyPage) {
    return <main className="min-h-screen bg-background">{children}</main>
  }

  return (
    <div className="min-h-screen bg-background flex">
      {/* Desktop sidebar */}
      <aside className="hidden md:flex flex-col w-56 border-r border-border bg-card fixed h-full z-20">
        <div className="p-5 border-b border-border">
          <div className="flex items-center gap-2">
            <div className="w-8 h-8 rounded-lg bg-primary flex items-center justify-center">
              <Grid3x3 className="h-4 w-4 text-primary-foreground" />
            </div>
            <span className="font-semibold text-sm">LED Matrix</span>
          </div>
        </div>
        <nav className="flex-1 p-3 space-y-1">
          {navItems.map((item) => (
            <Link
              key={item.to}
              to={item.to}
              className={cn(
                'flex items-center gap-3 px-3 py-2.5 rounded-lg text-sm font-medium transition-colors',
                location.pathname === item.to
                  ? 'bg-primary text-primary-foreground'
                  : 'text-muted-foreground hover:bg-secondary hover:text-foreground'
              )}
            >
              {item.icon}
              {item.label}
            </Link>
          ))}
        </nav>
        <div className="p-3 border-t border-border flex justify-end">
          <ThemeToggle />
        </div>
      </aside>

      {/* Mobile header */}
      <div className="md:hidden fixed top-0 left-0 right-0 h-14 border-b border-border bg-card flex items-center justify-between px-4 z-20">
        <div className="flex items-center gap-2">
          <div className="w-7 h-7 rounded-lg bg-primary flex items-center justify-center">
            <Grid3x3 className="h-3.5 w-3.5 text-primary-foreground" />
          </div>
          <span className="font-semibold text-sm">LED Matrix</span>
        </div>
        <ThemeToggle />
      </div>

      {/* Content */}
      <main className="flex-1 md:ml-56 pb-20 md:pb-0 pt-14 md:pt-0">
        <div className="max-w-5xl mx-auto p-4 md:p-6">
          {children}
        </div>
      </main>

      {/* Mobile bottom nav */}
      <nav className="md:hidden fixed bottom-0 left-0 right-0 bg-card border-t border-border flex z-20">
        {navItems.map((item) => (
          <Link
            key={item.to}
            to={item.to}
            className={cn(
              'flex-1 flex flex-col items-center justify-center py-2 gap-1 text-xs font-medium transition-colors',
              location.pathname === item.to
                ? 'text-primary'
                : 'text-muted-foreground hover:text-foreground'
            )}
          >
            {item.icon}
            {item.label}
          </Link>
        ))}
      </nav>
    </div>
  )
}
