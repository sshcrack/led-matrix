import type { Property } from '~/apiTypes/list_scenes'
import type { ListProviders } from '~/apiTypes/list_scenes'
import BooleanProperty from './properties/BooleanProperty'
import ColorProperty from './properties/ColorProperty'
import EnumProperty from './properties/EnumProperty'
import NumberProperty from './properties/NumberProperty'
import StringProperty from './properties/StringProperty'
import StringListProperty from './properties/StringListProperty'
import ProvidersProperty from './properties/ProvidersProperty'
import GeneralProperty from './properties/GeneralProperty'

interface PropertyListProps {
  properties: Property<any>[]
  arguments: Record<string, any>
  providers: ListProviders[]
  onChange: (args: Record<string, any>) => void
}

export default function PropertyList({ properties, arguments: args, providers, onChange }: PropertyListProps) {
  const update = (name: string, value: any) => {
    onChange({ ...args, [name]: value })
  }

  if (properties.length === 0) {
    return <p className="text-sm text-muted-foreground italic">No properties</p>
  }

  return (
    <div className="overflow-hidden rounded-xl border border-border/70 bg-background/45">
      {properties.map((prop, index) => {
        const value = args[prop.name] ?? prop.default_value
        const commonProps = {
          property: prop,
          value,
          onChange: (v: any) => update(prop.name, v),
        }

        const content = (() => {
          switch (prop.type_id) {
          case 'bool':
            return <BooleanProperty {...commonProps} />
          case 'color':
            return <ColorProperty {...commonProps} />
          case 'enum':
            return <EnumProperty {...commonProps} />
          case 'int':
          case 'double':
          case 'float':
          case 'int16_t':
          case 'uint8_t':
          case 'millis':
            return <NumberProperty {...commonProps} />
          case 'string':
            return <StringProperty {...commonProps} />
          case 'string[]':
            return <StringListProperty {...commonProps} />
          case 'json':
            return (
              <ProvidersProperty
                property={prop}
                value={value}
                providers={providers}
                onChange={(v) => update(prop.name, v)}
              />
            )
          default:
            return <GeneralProperty {...commonProps} />
          }
        })()

        return (
          <div
            key={prop.name}
            className={`p-4 sm:p-5 ${index > 0 ? 'border-t border-border/60' : ''}`}
          >
            {content}
          </div>
        )
      })}
    </div>
  )
}
