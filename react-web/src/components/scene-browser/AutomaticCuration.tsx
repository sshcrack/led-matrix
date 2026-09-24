import { EyeOff, Star, Wand2 } from 'lucide-react'
import type { AutomaticLook, AutomaticState } from '~/lib/automaticCuration'

const states: { value: AutomaticState; label: string; icon?: typeof Star }[] = [
  { value: 'favorite', label: 'Favorite', icon: Star },
  { value: 'on', label: 'On' },
  { value: 'off', label: 'Off', icon: EyeOff },
]

export default function AutomaticCuration({ looks, pending, onChange }: {
  looks: AutomaticLook[]
  pending: string | null
  onChange: (look: AutomaticLook, state: AutomaticState) => void
}) {
  if (looks.length === 0) return null
  return <div className="glass-panel rounded-2xl p-4">
    <div className="flex items-center gap-2 text-sm font-semibold"><Wand2 className="h-4 w-4 text-primary" />Automatic Mode</div>
    <p className="mt-1 text-xs text-muted-foreground">Choose which looks the Director may play. Favorites come up more often; Off keeps a look out of the rotation. It stays available for presets either way.</p>
    <div className="mt-3 space-y-2">
      {looks.map(look => <div key={look.key} className="flex items-center justify-between gap-3">
        <div className="min-w-0">
          <div className="truncate text-sm font-medium" title={look.description}>{look.label}</div>
          <div className="text-[11px] text-muted-foreground">{look.defaultOn ? 'On by default' : 'Off by default'}</div>
        </div>
        <div className="flex shrink-0 overflow-hidden rounded-full border border-border" role="radiogroup" aria-label={`Automatic Mode for ${look.label}`}>
          {states.map(({ value, label, icon: Icon }) => {
            const active = look.state === value
            return <button key={value} type="button" role="radio" aria-checked={active} disabled={pending === look.key}
              onClick={() => { if (!active) onChange(look, value) }}
              className={`flex items-center gap-1 px-2.5 py-1 text-xs font-medium transition disabled:opacity-50 ${active ? 'bg-primary text-primary-foreground' : 'text-muted-foreground hover:bg-secondary'}`}>
              {Icon && <Icon className={`h-3 w-3 ${active && value === 'favorite' ? 'fill-current' : ''}`} />}{label}
            </button>
          })}
        </div>
      </div>)}
    </div>
  </div>
}
