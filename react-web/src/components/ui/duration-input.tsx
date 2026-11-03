import { useEffect, useRef, useState } from 'react'
import { Clock3 } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { cn } from '~/lib/utils'
import { clamp, formatDuration, parseDuration } from '~/lib/duration'

interface DurationInputProps {
  id?: string
  label: string
  value: number
  onChange: (milliseconds: number) => void
  presets?: number[]
  min?: number
  max?: number
  description?: string
  className?: string
}

export default function DurationInput({
  id,
  label,
  value,
  onChange,
  presets = [],
  min = 0,
  max,
  description = 'Use a human duration such as 1.5s, 250ms, or 1m 30s.',
  className,
}: DurationInputProps) {
  const [draft, setDraft] = useState(() => formatDuration(value))
  const [error, setError] = useState<string | null>(null)
  const focused = useRef(false)

  useEffect(() => {
    if (!focused.current) setDraft(formatDuration(value))
  }, [value])

  const commit = () => {
    const parsed = parseDuration(draft)
    if (parsed === null) {
      setError('Enter a duration like 1.5s, 250ms, or 1m 30s.')
      return false
    }

    const next = clamp(parsed, min, max)
    onChange(next)
    setDraft(formatDuration(next))
    setError(null)
    return true
  }

  return (
    <div className={cn('space-y-2.5', className)}>
      <div className="flex items-center justify-between gap-3">
        <Label htmlFor={id} className="flex items-center gap-2">
          <Clock3 className="h-3.5 w-3.5 text-muted-foreground" />
          {label}
        </Label>
        <span className="text-xs tabular-nums text-muted-foreground">{Math.round(value)} ms</span>
      </div>

      <Input
        id={id}
        type="text"
        inputMode="decimal"
        value={draft}
        aria-invalid={Boolean(error)}
        className={cn('h-11 font-medium tabular-nums', error && 'border-destructive focus-visible:ring-destructive')}
        onFocus={() => { focused.current = true }}
        onChange={(event) => {
          setDraft(event.target.value)
          if (error) setError(null)
        }}
        onBlur={() => {
          focused.current = false
          if (!commit()) setDraft(formatDuration(value))
        }}
        onKeyDown={(event) => {
          if (event.key === 'Enter') {
            event.preventDefault()
            commit()
            event.currentTarget.blur()
          } else if (event.key === 'Escape') {
            setDraft(formatDuration(value))
            setError(null)
            event.currentTarget.blur()
          }
        }}
      />

      <div className="min-h-4 text-xs">
        {error ? <p className="text-destructive">{error}</p> : <p className="text-muted-foreground">{description}</p>}
      </div>

      {presets.length > 0 && (
        <div className="flex flex-wrap gap-2">
          {presets.map((preset) => (
            <Button
              key={preset}
              type="button"
              variant={value === preset ? 'default' : 'outline'}
              size="sm"
              className="h-8 rounded-full px-3 tabular-nums"
              onClick={() => {
                onChange(clamp(preset, min, max))
                setDraft(formatDuration(clamp(preset, min, max)))
                setError(null)
              }}
            >
              {formatDuration(preset)}
            </Button>
          ))}
        </div>
      )}
    </div>
  )
}
