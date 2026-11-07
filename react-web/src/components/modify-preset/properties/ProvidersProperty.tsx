import { Label } from '~/components/ui/label'
import { Badge } from '~/components/ui/badge'
import { titleCase } from '~/lib/utils'
import type { Property, ListProviders } from '~/apiTypes/list_scenes'
import { readSources } from '~/core/sceneSources'

interface ProvidersPropertyProps {
  property: Property<unknown>
  value: unknown
  providers: ListProviders[]
  onChange: (value: unknown) => void
}

/** Summary of a scene's sources; they are edited on the "Configure sources" page. */
export default function ProvidersProperty({ property, value }: ProvidersPropertyProps) {
  const sources = readSources(value)
  return (
    <div className="space-y-1.5">
      <Label>{property.additional?.label ?? titleCase(property.name)}</Label>
      <div className="flex flex-wrap items-center gap-2 p-2.5 rounded-lg border border-border bg-muted/30">
        {sources.length > 0
          ? sources.map(s => <Badge key={s.uuid} variant="outline" className="text-xs">{s.type}</Badge>)
          : <span className="text-sm text-muted-foreground italic">No sources configured</span>}
      </div>
      <p className="text-xs text-muted-foreground">Edit with "Configure sources" on the scene.</p>
    </div>
  )
}
