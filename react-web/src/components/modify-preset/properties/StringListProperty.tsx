import { useState } from 'react'
import { Plus, X } from 'lucide-react'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface StringListPropertyProps {
  property: Property<string[]>
  value: string[]
  onChange: (value: string[]) => void
}

export default function StringListProperty({ property, value, onChange }: StringListPropertyProps) {
  const [input, setInput] = useState('')

  const list = Array.isArray(value) ? value : []

  const add = () => {
    const v = input.trim()
    if (v && !list.includes(v)) {
      onChange([...list, v])
      setInput('')
    }
  }

  const remove = (item: string) => {
    onChange(list.filter(v => v !== item))
  }

  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <div className="flex gap-2">
        <Input
          value={input}
          onChange={(e) => setInput(e.target.value)}
          placeholder="Add item..."
          onKeyDown={(e) => e.key === 'Enter' && add()}
        />
        <Button variant="outline" size="icon" onClick={add} type="button">
          <Plus className="h-4 w-4" />
        </Button>
      </div>
      {list.length > 0 && (
        <div className="flex flex-wrap gap-1.5 mt-2">
          {list.map((item, i) => (
            <Badge key={i} variant="secondary" className="gap-1 pr-1">
              <span className="truncate max-w-[200px]">{item}</span>
              <button
                type="button"
                onClick={() => remove(item)}
                className="ml-1 hover:text-destructive transition-colors"
              >
                <X className="h-3 w-3" />
              </button>
            </Badge>
          ))}
        </div>
      )}
    </div>
  )
}
