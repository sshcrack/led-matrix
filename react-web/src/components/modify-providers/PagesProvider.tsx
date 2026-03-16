import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import type { PagesProvider as PagesProviderType } from '~/apiTypes/list_scenes'

interface PagesProviderProps {
  provider: PagesProviderType
  onChange: (provider: PagesProviderType) => void
}

export default function PagesProvider({ provider, onChange }: PagesProviderProps) {
  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="text-sm">Pages Provider</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="grid grid-cols-2 gap-3">
          <div className="space-y-1.5">
            <Label>Begin</Label>
            <Input
              type="number"
              min={0}
              value={provider.arguments.begin}
              onChange={e => onChange({ ...provider, arguments: { ...provider.arguments, begin: Number(e.target.value) } })}
            />
          </div>
          <div className="space-y-1.5">
            <Label>End</Label>
            <Input
              type="number"
              min={0}
              value={provider.arguments.end}
              onChange={e => onChange({ ...provider, arguments: { ...provider.arguments, end: Number(e.target.value) } })}
            />
          </div>
        </div>
      </CardContent>
    </Card>
  )
}
