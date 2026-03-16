import { Trash2 } from 'lucide-react'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { CollectionProviderComponent } from './collection/CollectionProvider'
import PagesProvider from './PagesProvider'
import type { ProviderValue, ListProviders, CollectionProvider, PagesProvider as PagesProviderType } from '~/apiTypes/list_scenes'

interface ImageProvidersProps {
  providers: ProviderValue[]
  providerDefinitions: ListProviders[]
  onChange: (providers: ProviderValue[]) => void
}

export default function ImageProviders({ providers, providerDefinitions, onChange }: ImageProvidersProps) {
  const updateProvider = (index: number, updated: ProviderValue) => {
    const newProviders = [...providers]
    newProviders[index] = updated
    onChange(newProviders)
  }

  const removeProvider = (index: number) => {
    onChange(providers.filter((_, i) => i !== index))
  }

  return (
    <div className="space-y-3">
      {providers.map((provider, i) => (
        <div key={provider.uuid ?? i} className="relative">
          <Button
            variant="ghost"
            size="icon"
            className="absolute top-3 right-3 z-10 h-7 w-7 text-destructive hover:text-destructive"
            onClick={() => removeProvider(i)}
          >
            <Trash2 className="h-3.5 w-3.5" />
          </Button>
          {provider.type === 'collection' ? (
            <CollectionProviderComponent
              provider={provider as CollectionProvider}
              onChange={p => updateProvider(i, p)}
            />
          ) : provider.type === 'pages' ? (
            <PagesProvider
              provider={provider as PagesProviderType}
              onChange={p => updateProvider(i, p)}
            />
          ) : (
            <Card>
              <CardContent className="p-4">
                <Badge variant="secondary">{provider.type}</Badge>
                <p className="text-xs text-muted-foreground mt-2">
                  Unknown provider type
                </p>
              </CardContent>
            </Card>
          )}
        </div>
      ))}
    </div>
  )
}
