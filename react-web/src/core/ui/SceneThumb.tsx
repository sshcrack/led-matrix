import { ImageOff } from 'lucide-react'
import { cn } from '~/lib/utils'
import { useMatrix } from '../matrixContext'
import { scenePreviewUrl } from '../scenes'

/** Square pixel-art preview of a scene (or a placeholder when none exists). */
export function SceneThumb({ name, className }: { name: string; className?: string }) {
  const { apiUrl, sceneByName } = useMatrix()
  const has = sceneByName.get(name)?.has_preview
  return <div className={cn('grid aspect-square place-items-center overflow-hidden bg-black text-white/30', className)}>
    {has ? <img loading="lazy" src={scenePreviewUrl(apiUrl, name)} alt="" className="h-full w-full object-cover [image-rendering:pixelated]" />
      : <ImageOff className="h-1/3 w-1/3" />}
  </div>
}

/** Overlapping film-strip of up to three scene thumbnails for a preset. */
export function ThumbStrip({ names, size = 'h-10 w-10', className }: { names: string[]; size?: string; className?: string }) {
  if (names.length === 0) return <SceneThumb name="" className={cn(size, 'rounded-lg', className)} />
  return <div className={cn('flex -space-x-3', className)}>
    {names.map((n, i) => <SceneThumb key={n + i} name={n} className={cn(size, 'rounded-lg ring-2 ring-card')} />)}
  </div>
}
