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
    <div className="space-y-3">
      {properties.map((prop) => {
        const value = args[prop.name] ?? prop.default_value
        const commonProps = {
          property: prop,
          value,
          onChange: (v: any) => update(prop.name, v),
        }

        switch (prop.type_id) {
          case 'bool':
            return <BooleanProperty key={prop.name} {...commonProps} />
          case 'color':
            return <ColorProperty key={prop.name} {...commonProps} />
          case 'enum':
            return <EnumProperty key={prop.name} {...commonProps} />
          case 'int':
          case 'double':
          case 'float':
          case 'int16_t':
          case 'uint8_t':
          case 'millis':
            return <NumberProperty key={prop.name} {...commonProps} />
          case 'string':
            return <StringProperty key={prop.name} {...commonProps} />
          case 'string[]':
            return <StringListProperty key={prop.name} {...commonProps} />
          case 'json':
            return (
              <ProvidersProperty
                key={prop.name}
                property={prop}
                value={value}
                providers={providers}
                onChange={(v) => update(prop.name, v)}
              />
            )
          default:
            return <GeneralProperty key={prop.name} {...commonProps} />
        }
      })}
    </div>
  )
}
