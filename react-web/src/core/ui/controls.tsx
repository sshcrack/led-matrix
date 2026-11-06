import { Check, Loader2, Moon, Power, Sparkles, TriangleAlert, Hand } from 'lucide-react'
import { cn } from '~/lib/utils'
import { useMatrix } from '../matrixContext'
import type { SaveState } from '../usePresetEditor'

export function PowerButton({ className, labelled }: { className?: string; labelled?: boolean }) {
  const { status, setPower } = useMatrix()
  const off = status?.turned_off ?? false
  return <button onClick={() => void setPower(off)} aria-pressed={!off} aria-label={off ? 'Turn matrix on' : 'Turn matrix off'}
    className={cn('inline-flex items-center justify-center gap-2 rounded-full font-medium transition', off ? 'bg-secondary text-muted-foreground hover:bg-secondary/70' : 'bg-success text-success-foreground shadow-md shadow-success/30 hover:opacity-90', labelled ? 'h-10 px-4 text-sm' : 'h-10 w-10', className)}>
    {off ? <Moon className="h-4 w-4" /> : <Power className="h-4 w-4" />}{labelled && (off ? 'Off' : 'On')}
  </button>
}

/** Auto / Manual as a segmented control: one tap, both options visible. */
export function ModeSwitch({ className }: { className?: string }) {
  const { status, setMode } = useMatrix()
  const auto = status?.operation_mode === 'automatic'
  const seg = (active: boolean) => cn('inline-flex items-center gap-1.5 rounded-full px-3 py-1.5 text-xs font-semibold transition', active ? 'bg-primary text-primary-foreground shadow' : 'text-muted-foreground hover:text-foreground')
  return <div role="radiogroup" aria-label="Operation mode" className={cn('inline-flex rounded-full bg-secondary p-1', className)}>
    <button role="radio" aria-checked={auto} className={seg(auto)} onClick={() => void setMode('automatic')}><Sparkles className="h-3.5 w-3.5" />Auto</button>
    <button role="radio" aria-checked={!auto} className={seg(!auto)} onClick={() => void setMode('manual')}><Hand className="h-3.5 w-3.5" />Manual</button>
  </div>
}

export function SaveBadge({ state, onRetry, className }: { state: SaveState; onRetry?: () => void; className?: string }) {
  const map = {
    saved: { icon: <Check className="h-3.5 w-3.5" />, text: 'Saved', tone: 'text-muted-foreground' },
    dirty: { icon: <Loader2 className="h-3.5 w-3.5" />, text: 'Editing…', tone: 'text-muted-foreground' },
    saving: { icon: <Loader2 className="h-3.5 w-3.5 animate-spin" />, text: 'Saving…', tone: 'text-muted-foreground' },
    error: { icon: <TriangleAlert className="h-3.5 w-3.5" />, text: 'Not saved — retry', tone: 'text-destructive' },
  }[state]
  return <button disabled={state !== 'error'} onClick={onRetry} aria-live="polite" className={cn('inline-flex items-center gap-1.5 text-xs font-medium', map.tone, className)}>{map.icon}{map.text}</button>
}
