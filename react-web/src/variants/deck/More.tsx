import { Link } from 'react-router-dom'
import { ChevronRight } from 'lucide-react'
import { NAV } from '~/core/nav'

export default function DeckMore() {
  return <div className="space-y-3 pt-2"><h1 className="text-2xl font-extrabold tracking-tight">More</h1>
    <ul className="overflow-hidden rounded-3xl bg-card ring-1 ring-border">
      {NAV.filter(n => !['/', '/gallery'].includes(n.to)).map((n, i) => <li key={n.to} className={i ? 'border-t border-border' : ''}>
        <Link to={n.to} className="flex items-center gap-3 p-4 active:bg-secondary"><span className="grid h-10 w-10 place-items-center rounded-2xl bg-primary/12 text-primary">{n.icon}</span>
          <span className="flex-1"><span className="block font-bold">{n.label}</span><span className="block text-xs text-muted-foreground">{n.description}</span></span><ChevronRight className="h-4 w-4 text-muted-foreground" /></Link></li>)}
    </ul></div>
}
