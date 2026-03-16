import { useState } from 'react'
import { ImageOff } from 'lucide-react'

interface ScenePreviewProps {
  sceneName: string
  apiUrl: string
  /** Extra Tailwind classes forwarded to the wrapper element */
  className?: string
  /** When true renders a small inline thumbnail (for use inside scene cards) */
  compact?: boolean
}

/**
 * Attempts to load a scene preview image from the matrix controller.
 * Falls back gracefully when no preview is available.
 */
export default function ScenePreview({ sceneName, apiUrl, className = '', compact = false }: ScenePreviewProps) {
  const [failed, setFailed] = useState(false)

  const src = `${apiUrl}/scene_preview/${encodeURIComponent(sceneName)}`

  if (failed) {
    if (compact) return null
    return (
      <div className={`flex items-center justify-center bg-muted text-muted-foreground gap-2 text-sm p-4 ${className}`}>
        <ImageOff className="h-4 w-4" />
        No preview available
      </div>
    )
  }

  if (compact) {
    return (
      <img
        src={src}
        alt={`${sceneName} preview`}
        className={`object-cover ${className}`}
        onError={() => setFailed(true)}
      />
    )
  }

  return (
    <div className={`bg-black flex items-center justify-center ${className}`}>
      <img
        src={src}
        alt={`${sceneName} preview`}
        className="max-w-full max-h-48 object-contain"
        style={{ imageRendering: 'pixelated' }}
        onError={() => setFailed(true)}
      />
    </div>
  )
}
