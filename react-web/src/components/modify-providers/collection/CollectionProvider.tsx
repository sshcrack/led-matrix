import { useState, useRef } from 'react'
import { Upload, Plus, Loader2 } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card'
import { CollectionItem } from './CollectionItem'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import { toast } from 'sonner'
import type { CollectionProvider } from '~/apiTypes/list_scenes'

interface CollectionProviderComponentProps {
  provider: CollectionProvider
  onChange: (provider: CollectionProvider) => void
}

export function CollectionProviderComponent({ provider, onChange }: CollectionProviderComponentProps) {
  const [urlInput, setUrlInput] = useState('')
  const [uploading, setUploading] = useState(false)
  const fileRef = useRef<HTMLInputElement>(null)
  const apiUrl = useApiUrl()

  const images = provider.arguments.images ?? []

  const addUrl = () => {
    const url = urlInput.trim()
    if (url) {
      onChange({ ...provider, arguments: { images: [...images, url] } })
      setUrlInput('')
    }
  }

  const removeImage = (index: number) => {
    const newImages = images.filter((_, i) => i !== index)
    onChange({ ...provider, arguments: { images: newImages } })
  }

  const handleFileUpload = async (files: FileList | null) => {
    if (!files || !apiUrl) return
    setUploading(true)
    const newUrls: string[] = []
    for (const file of Array.from(files)) {
      try {
        const formData = new FormData()
        formData.append('file', file)
        const res = await fetch(`${apiUrl}/upload`, { method: 'POST', body: formData })
        const data = await res.json()
        if (data.url) newUrls.push(data.url)
      } catch {
        toast.error(`Failed to upload ${file.name}`)
      }
    }
    if (newUrls.length > 0) {
      onChange({ ...provider, arguments: { images: [...images, ...newUrls] } })
      toast.success(`Uploaded ${newUrls.length} image(s)`)
    }
    setUploading(false)
  }

  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="text-sm">Image Collection</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div className="space-y-2">
          <Label>Add image URL</Label>
          <div className="flex gap-2">
            <Input
              placeholder="https://... or /path/to/image.png"
              value={urlInput}
              onChange={e => setUrlInput(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && addUrl()}
            />
            <Button variant="outline" size="icon" onClick={addUrl}>
              <Plus className="h-4 w-4" />
            </Button>
          </div>
        </div>

        <div>
          <input
            ref={fileRef}
            type="file"
            accept="image/*"
            multiple
            className="hidden"
            onChange={e => handleFileUpload(e.target.files)}
          />
          <Button
            variant="outline"
            size="sm"
            className="gap-2 w-full"
            onClick={() => fileRef.current?.click()}
            disabled={uploading}
          >
            {uploading ? <Loader2 className="h-4 w-4 animate-spin" /> : <Upload className="h-4 w-4" />}
            Upload Images
          </Button>
        </div>

        {images.length > 0 ? (
          <div className="space-y-2">
            {images.map((url, i) => (
              <CollectionItem key={i} url={url} onRemove={() => removeImage(i)} />
            ))}
          </div>
        ) : (
          <p className="text-sm text-muted-foreground text-center py-4">No images added</p>
        )}
      </CardContent>
    </Card>
  )
}
