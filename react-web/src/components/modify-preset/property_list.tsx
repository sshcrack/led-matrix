import { useState } from 'react'
import { ChevronDown, SlidersHorizontal } from 'lucide-react'
import type { Property, ListProviders } from '~/apiTypes/list_scenes'
import { Button } from '~/components/ui/button'
import BooleanProperty from './properties/BooleanProperty'
import ColorProperty from './properties/ColorProperty'
import EnumProperty from './properties/EnumProperty'
import NumberProperty from './properties/NumberProperty'
import StringProperty from './properties/StringProperty'
import StringListProperty from './properties/StringListProperty'
import ProvidersProperty from './properties/ProvidersProperty'
import GeneralProperty from './properties/GeneralProperty'
import AudioModulationProperty from './properties/AudioModulationProperty'
import { propertyGroup, propertyVisible } from './propertyUi'

interface PropertyListProps {
  properties: Property<any>[]
  arguments: Record<string, any>
  providers: ListProviders[]
  onChange: (args: Record<string, any>) => void
}

export default function PropertyList({ properties, arguments: args, providers, onChange }: PropertyListProps) {
  const [showAdvanced, setShowAdvanced] = useState(false)
  const update = (name: string, value: any) => onChange({ ...args, [name]: value })

  const visible = properties.filter((property) => propertyVisible(property, args))
  const normal = visible.filter((property) => !property.additional?.advanced)
  const advanced = visible.filter((property) => property.additional?.advanced)
  const displayed = showAdvanced ? [...normal, ...advanced] : normal

  if (properties.length === 0) return <p className="text-sm text-muted-foreground italic">No properties</p>

  const groups = new Map<string, Property<any>[]>()
  for (const property of displayed) {
    const group = propertyGroup(property)
    const list = groups.get(group) ?? []
    list.push(property)
    groups.set(group, list)
  }

  const renderProperty = (prop: Property<any>) => {
    const value = args[prop.name] ?? prop.default_value
    const commonProps = { property: prop, value, onChange: (v: any) => update(prop.name, v) }
    if (prop.type_id === 'json' && prop.additional?.control === 'audio_modulations') {
      return <AudioModulationProperty
        property={prop}
        value={value}
        properties={properties}
        onChange={(next) => update(prop.name, next)}
      />
    }
    switch (prop.type_id) {
      case 'bool': return <BooleanProperty {...commonProps} />
      case 'color': return <ColorProperty {...commonProps} />
      case 'enum': return <EnumProperty {...commonProps} />
      case 'int': case 'double': case 'float': case 'int16_t': case 'uint8_t': case 'millis':
        return <NumberProperty {...commonProps} />
      case 'string': return <StringProperty {...commonProps} />
      case 'string[]': return <StringListProperty {...commonProps} />
      case 'json': return <ProvidersProperty property={prop} value={value} providers={providers} onChange={(v) => update(prop.name, v)} />
      default: return <GeneralProperty {...commonProps} />
    }
  }

  return (
    <div className="space-y-3">
      {[...groups.entries()].map(([group, groupProperties]) => (
        <section key={group} className="overflow-hidden rounded-xl border border-border/70 bg-background/45">
          <div className="flex items-center gap-2 border-b border-border/60 bg-muted/20 px-4 py-2.5">
            <SlidersHorizontal className="h-3.5 w-3.5 text-muted-foreground" />
            <h4 className="text-xs font-semibold uppercase tracking-[0.12em] text-muted-foreground">{group}</h4>
          </div>
          {groupProperties.map((prop, index) => (
            <div key={prop.name} className={`p-4 sm:p-5 ${index > 0 ? 'border-t border-border/60' : ''}`}>
              {renderProperty(prop)}
            </div>
          ))}
        </section>
      ))}

      {advanced.length > 0 && (
        <Button type="button" variant="ghost" size="sm" className="gap-2 text-muted-foreground" onClick={() => setShowAdvanced((value) => !value)}>
          <ChevronDown className={`h-4 w-4 transition-transform ${showAdvanced ? 'rotate-180' : ''}`} />
          {showAdvanced ? 'Hide advanced settings' : `Show ${advanced.length} advanced setting${advanced.length === 1 ? '' : 's'}`}
        </Button>
      )}
    </div>
  )
}
