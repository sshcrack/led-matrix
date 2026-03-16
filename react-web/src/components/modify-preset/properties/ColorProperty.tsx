import { useState } from 'react'
import { HexColorPicker } from 'react-colorful'
import { Label } from '~/components/ui/label'
import { Input } from '~/components/ui/input'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface ColorPropertyProps {
  property: Property<string>
  value: string
  onChange: (value: string) => void
}

function normalizeHex(color: any): string {
  if (typeof color === 'string') {
    if (color.startsWith('#')) return color
    return '#' + color
  }
  if (typeof color === 'number') {
    return '#' + color.toString(16).padStart(6, '0')
  }
  return '#000000'
}

function toHex(color: string): string {
  return color.startsWith('#') ? color : '#' + color
}

export default function ColorProperty({ property, value, onChange }: ColorPropertyProps) {
  const [open, setOpen] = useState(false)
  const hexColor = normalizeHex(value)

  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <div className="flex items-center gap-2">
        <button
          type="button"
          className="h-9 w-9 rounded-lg border border-border shadow-sm flex-shrink-0 transition-all hover:scale-110"
          style={{ backgroundColor: hexColor }}
          onClick={() => setOpen(!open)}
          aria-label="Pick color"
        />
        <Input
          value={hexColor}
          onChange={(e) => onChange(e.target.value)}
          placeholder="#000000"
          className="font-mono"
          maxLength={7}
        />
      </div>
      {open && (
        <div className="mt-2 p-3 border border-border rounded-xl bg-card shadow-md">
          <HexColorPicker
            color={hexColor}
            onChange={(c) => onChange(c)}
          />
          <button
            className="mt-2 text-xs text-muted-foreground hover:text-foreground"
            onClick={() => setOpen(false)}
          >
            Close
          </button>
        </div>
      )}
    </div>
  )
}
