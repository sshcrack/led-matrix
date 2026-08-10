import { Expand, ImageOff, MonitorPlay } from 'lucide-react'
import { useEffect, useRef, useState } from 'react'
import { cn } from '~/lib/utils'

interface LiveMatrixPreviewProps {
  apiUrl: string
  fallbackSceneName?: string | null
  fallbackHasPreview?: boolean
  className?: string
  label?: string
}

type LiveState = 'connecting' | 'live' | 'fallback'

const delay = (ms: number) => new Promise(resolve => window.setTimeout(resolve, ms))

function drawFrame(canvas: HTMLCanvasElement, buffer: ArrayBuffer) {
  const bytes = new Uint8Array(buffer)
  if (bytes.length < 12 || bytes[0] !== 0x4c || bytes[1] !== 0x4d || bytes[2] !== 0x46 || bytes[3] !== 0x31)
    throw new Error('Unsupported live frame payload')

  const view = new DataView(buffer)
  const width = view.getUint16(4, true)
  const height = view.getUint16(6, true)
  const sequence = view.getUint32(8, true)
  const expected = 12 + width * height * 3
  if (!width || !height || bytes.length !== expected)
    throw new Error('Incomplete live frame payload')

  if (canvas.width !== width) canvas.width = width
  if (canvas.height !== height) canvas.height = height
  const context = canvas.getContext('2d', { alpha: false })
  if (!context) throw new Error('Canvas 2D context unavailable')

  const rgba = new Uint8ClampedArray(width * height * 4)
  for (let source = 12, target = 0; source < bytes.length; source += 3, target += 4) {
    rgba[target] = bytes[source]
    rgba[target + 1] = bytes[source + 1]
    rgba[target + 2] = bytes[source + 2]
    rgba[target + 3] = 255
  }
  context.putImageData(new ImageData(rgba, width, height), 0, 0)
  return { width, height, sequence }
}

export default function LiveMatrixPreview({
  apiUrl,
  fallbackSceneName,
  fallbackHasPreview,
  className,
  label,
}: LiveMatrixPreviewProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const shellRef = useRef<HTMLDivElement>(null)
  const [state, setState] = useState<LiveState>('connecting')
  const [dimensions, setDimensions] = useState({ width: 128, height: 128 })

  useEffect(() => {
    if (!apiUrl) return

    let cancelled = false
    let controller: AbortController | null = null

    const poll = async () => {
      let failures = 0
      while (!cancelled) {
        const shell = shellRef.current
        const rect = shell?.getBoundingClientRect()
        const visible = document.visibilityState !== 'hidden' && Boolean(
          rect && rect.bottom > 0 && rect.top < window.innerHeight && rect.right > 0 && rect.left < window.innerWidth,
        )
        if (!visible) {
          // No HTTP request means no matrix-frame copy on the Pi. This also
          // stops captures when the browser tab is hidden or the preview has
          // been scrolled completely off screen.
          await delay(500)
          continue
        }

        controller = new AbortController()
        try {
          const response = await fetch(`${apiUrl}/live_frame`, {
            cache: 'no-store',
            signal: controller.signal,
          })
          if (response.status === 204) {
            if (failures === 0) setState('connecting')
            await delay(90)
            continue
          }
          if (!response.ok) throw new Error(`Live frame request failed (${response.status})`)

          const buffer = await response.arrayBuffer()
          if (!canvasRef.current) return
          const frame = drawFrame(canvasRef.current, buffer)
          failures = 0
          setDimensions({ width: frame.width, height: frame.height })
          setState('live')
          await delay(75)
        } catch (error) {
          if (cancelled || (error instanceof DOMException && error.name === 'AbortError')) return
          failures += 1
          setState('fallback')
          await delay(Math.min(2500, 500 + failures * 350))
        }
      }
    }

    void poll()
    return () => {
      cancelled = true
      controller?.abort()
    }
  }, [apiUrl])

  const enterFullscreen = async () => {
    const shell = shellRef.current
    if (!shell) return
    if (document.fullscreenElement) await document.exitFullscreen()
    else await shell.requestFullscreen?.()
  }

  const fallbackAvailable = Boolean(fallbackSceneName && fallbackHasPreview)
  const statusLabel = state === 'live' ? 'Live' : state === 'connecting' ? 'Connecting' : 'Generated preview'

  return (
    <div ref={shellRef} className={cn('matrix-shell', className)}>
      <div className="matrix-toolbar">
        <div className="flex min-w-0 items-center gap-2">
          <span className={cn(
            'h-2 w-2 shrink-0 rounded-full',
            state === 'live' && 'bg-emerald-400 shadow-[0_0_12px_rgba(52,211,153,0.9)]',
            state === 'connecting' && 'animate-pulse bg-amber-300 shadow-[0_0_10px_rgba(252,211,77,0.7)]',
            state === 'fallback' && 'bg-white/25',
          )} />
          <span className="truncate">{label ?? 'Live matrix'}</span>
          <span className="hidden rounded-full bg-white/5 px-2 py-0.5 text-[9px] uppercase tracking-[0.16em] text-white/45 sm:inline">{statusLabel}</span>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-[10px] uppercase tracking-[0.18em] text-white/40">{dimensions.width} × {dimensions.height}</span>
          <button type="button" onClick={() => void enterFullscreen()} className="rounded-md p-1.5 text-white/45 transition hover:bg-white/10 hover:text-white" aria-label="Toggle fullscreen matrix preview" title="Fullscreen">
            <Expand className="h-3.5 w-3.5" />
          </button>
        </div>
      </div>
      <div className="matrix-stage">
        <div className="matrix-grid-overlay" />
        <canvas ref={canvasRef} className={cn('relative z-10 h-full w-full [image-rendering:pixelated]', state !== 'live' && 'invisible')} />

        {state === 'fallback' && fallbackAvailable && (
          <img
            src={`${apiUrl}/scene_preview?name=${encodeURIComponent(fallbackSceneName!)}`}
            alt={`${fallbackSceneName} generated scene preview`}
            className="matrix-preview-image absolute inset-0"
          />
        )}

        {state !== 'live' && !fallbackAvailable && (
          <div className="absolute inset-0 z-10 flex h-full flex-col items-center justify-center gap-3 text-white/45">
            {state === 'connecting' ? <MonitorPlay className="h-9 w-9 animate-pulse" /> : <ImageOff className="h-8 w-8" />}
            <div className="max-w-56 text-center text-xs leading-relaxed">
              {state === 'connecting'
                ? 'Waiting for the first rendered matrix frame…'
                : 'Live frame streaming is unavailable on this controller.'}
            </div>
          </div>
        )}
      </div>
    </div>
  )
}
