import { ImageOff, MonitorPlay } from 'lucide-react'
import { useState } from 'react'
import { cn } from '~/lib/utils'

interface MatrixPreviewProps {
  apiUrl: string
  sceneName?: string | null
  hasPreview?: boolean
  className?: string
  label?: string
}

export default function MatrixPreview({ apiUrl, sceneName, hasPreview, className, label }: MatrixPreviewProps) {
  const [failed, setFailed] = useState(false)
  const showImage = Boolean(sceneName && hasPreview && !failed)

  return (
    <div className={cn('matrix-shell', className)}>
      <div className="matrix-toolbar">
        <div className="flex items-center gap-2 min-w-0">
          <span className="matrix-live-dot" />
          <span className="truncate">{label ?? sceneName ?? 'Matrix preview'}</span>
        </div>
        <span className="text-[10px] uppercase tracking-[0.18em] text-white/40">128 × 128</span>
      </div>
      <div className="matrix-stage">
        <div className="matrix-grid-overlay" />
        {showImage ? (
          <img
            key={sceneName}
            src={`${apiUrl}/scene_preview?name=${encodeURIComponent(sceneName!)}`}
            alt={`${sceneName} scene preview`}
            className="matrix-preview-image"
            onError={() => setFailed(true)}
          />
        ) : (
          <div className="relative z-10 flex h-full flex-col items-center justify-center gap-3 text-white/45">
            {sceneName ? <ImageOff className="h-8 w-8" /> : <MonitorPlay className="h-9 w-9" />}
            <div className="max-w-48 text-center text-xs leading-relaxed">
              {sceneName ? 'No generated preview is available for this scene yet.' : 'Choose a scene to preview it on the virtual matrix.'}
            </div>
          </div>
        )}
      </div>
    </div>
  )
}
