import { useState } from 'react'
import { Plus, X } from 'lucide-react'
import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card'
import { Input } from '~/components/ui/input'
import { Button } from '~/components/ui/button'
import type { CollectionShaderProvider as CollectionShaderProviderType } from '~/apiTypes/list_scenes'

interface CollectionShaderProviderProps {
  provider: CollectionShaderProviderType
  onChange: (provider: CollectionShaderProviderType) => void
}

export default function CollectionShaderProvider({ provider, onChange }: CollectionShaderProviderProps) {
  const [input, setInput] = useState('')
  const urls = provider.arguments.urls ?? []

  const addUrl = () => {
    const url = input.trim()
    if (url) {
      onChange({ ...provider, arguments: { urls: [...urls, url] } })
      setInput('')
    }
  }

  const removeUrl = (index: number) => {
    const newUrls = urls.filter((_, i) => i !== index)
    onChange({ ...provider, arguments: { urls: newUrls } })
  }

  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="text-sm">Shader Collection</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="flex gap-2">
          <Input
            placeholder="https://shadertoy.com/view/..."
            value={input}
            onChange={e => setInput(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && addUrl()}
          />
          <Button variant="outline" size="icon" onClick={addUrl}>
            <Plus className="h-4 w-4" />
          </Button>
        </div>
        {urls.length > 0 ? (
          <div className="space-y-2">
            {urls.map((url, i) => (
              <div key={i} className="flex items-center gap-2 p-2 rounded-lg border border-border">
                <span className="flex-1 text-xs text-muted-foreground truncate">{url}</span>
                <Button
                  variant="ghost"
                  size="icon"
                  className="h-6 w-6 text-destructive"
                  onClick={() => removeUrl(i)}
                >
                  <X className="h-3 w-3" />
                </Button>
              </div>
            ))}
          </div>
        ) : (
          <p className="text-sm text-muted-foreground text-center py-4">No shader URLs</p>
        )}
      </CardContent>
    </Card>
  )
}
