import { Label } from '~/components/ui/label'
import { Badge } from '~/components/ui/badge'
import { titleCase } from '~/lib/utils'
import type { Property, ListProviders, ProviderValue } from '~/apiTypes/list_scenes'

interface ProvidersPropertyProps {
  property: Property<any>
  value: ProviderValue | null
  providers: ListProviders[]
  onChange: (value: any) => void
}

export default function ProvidersProperty({ property, value }: ProvidersPropertyProps) {
  const providerType = property.additional?.provider_type ?? 'unknown'
  const hasValue = value && typeof value === 'object' && 'type' in value

  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <div className="flex items-center gap-2 p-2.5 rounded-lg border border-border bg-muted/30">
        <Badge variant="outline" className="text-xs">{providerType}</Badge>
        {hasValue ? (
          <span className="text-sm text-muted-foreground">
            {(value as ProviderValue).type}
          </span>
        ) : (
          <span className="text-sm text-muted-foreground italic">No provider configured</span>
        )}
      </div>
      <p className="text-xs text-muted-foreground">
        Edit via the "Edit Providers" button below
      </p>
    </div>
  )
}
