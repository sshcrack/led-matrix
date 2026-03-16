import { useState } from 'react'
import { Trash2, ImageIcon } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'

interface CollectionItemProps {
  url: string
  onRemove: () => void
}

export function CollectionItem({ url, onRemove }: CollectionItemProps) {
  const apiUrl = useApiUrl()
  const imageUrl = apiUrl ? `${apiUrl}/image?url=${encodeURIComponent(url)}` : ''

  return (
    <div className="flex items-center gap-2 p-2 rounded-lg border border-border bg-card">
      <div className="h-10 w-10 rounded-md overflow-hidden bg-muted flex-shrink-0">
        {imageUrl && (
          <img src={imageUrl} alt={url} className="h-full w-full object-cover" />
        )}
      </div>
      <span className="flex-1 text-xs text-muted-foreground truncate">{url}</span>
      <Button variant="ghost" size="icon" className="h-7 w-7 text-destructive hover:text-destructive" onClick={onRemove}>
        <Trash2 className="h-3.5 w-3.5" />
      </Button>
    </div>
  )
}
