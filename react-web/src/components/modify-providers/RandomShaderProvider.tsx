import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import type { RandomShaderProvider as RandomShaderProviderType } from '~/apiTypes/list_scenes'

interface RandomShaderProviderProps {
  provider: RandomShaderProviderType
  onChange: (provider: RandomShaderProviderType) => void
}

export default function RandomShaderProvider({ provider, onChange }: RandomShaderProviderProps) {
  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="text-sm">Random Shader Provider</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="grid grid-cols-2 gap-3">
          <div className="space-y-1.5">
            <Label>Min Page</Label>
            <Input
              type="number"
              min={0}
              value={provider.arguments.min_page}
              onChange={e => onChange({ ...provider, arguments: { ...provider.arguments, min_page: Number(e.target.value) } })}
            />
          </div>
          <div className="space-y-1.5">
            <Label>Max Page</Label>
            <Input
              type="number"
              min={0}
              value={provider.arguments.max_page}
              onChange={e => onChange({ ...provider, arguments: { ...provider.arguments, max_page: Number(e.target.value) } })}
            />
          </div>
        </div>
      </CardContent>
    </Card>
  )
}
