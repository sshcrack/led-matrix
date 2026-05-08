import { useEffect, useRef, useState } from 'react'
import { load } from 'fengari-web'

interface LuaPreviewProps {
  apiUrl: string
  filename: string | null
}

const PREVIEW_SIZE = 128
const PIXEL_SCALE = 3

export default function LuaPreview({ apiUrl, filename }: LuaPreviewProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [status, setStatus] = useState('Select a Lua script to preview')

  useEffect(() => {
    let rafId = 0
    let cancelled = false
    const canvas = canvasRef.current
    const ctx = canvas?.getContext('2d')
    if (!canvas || !ctx || !filename) {
      return
    }

    const width = PREVIEW_SIZE
    const height = PREVIEW_SIZE

    const setPixel = (x: number, y: number, r: number, g: number, b: number) => {
      if (x < 0 || y < 0 || x >= width || y >= height) return
      ctx.fillStyle = `rgb(${Math.max(0, Math.min(255, r))}, ${Math.max(0, Math.min(255, g))}, ${Math.max(0, Math.min(255, b))})`
      ctx.fillRect(x * PIXEL_SCALE, y * PIXEL_SCALE, PIXEL_SCALE, PIXEL_SCALE)
    }

    const matrix = {
      width: () => width,
      height: () => height,
      setPixel,
      fill: (r: number = 0, g: number = 0, b: number = 0) => {
        ctx.fillStyle = `rgb(${Math.max(0, Math.min(255, r))}, ${Math.max(0, Math.min(255, g))}, ${Math.max(0, Math.min(255, b))})`
        ctx.fillRect(0, 0, width * PIXEL_SCALE, height * PIXEL_SCALE)
      },
    }

    const start = async () => {
      try {
        setError(null)
        setStatus(`Loading ${filename}...`)
        const res = await fetch(`${apiUrl}/api/custom-assets/lua/${encodeURIComponent(filename)}/download`)
        if (!res.ok) {
          throw new Error(`Failed to fetch script (${res.status})`)
        }
        const script = await res.text()
        if (cancelled) return

        ;(globalThis as any).matrix = matrix
        matrix.fill(0, 0, 0)
        load(script)()
        setStatus(`Previewing ${filename}`)

        const tick = () => {
          if (cancelled) return
          try {
            load('if update then update() end')()
            load('if draw then draw() end')()
          } catch (e: any) {
            setError(e?.message ?? 'Lua runtime error')
            return
          }
          rafId = requestAnimationFrame(tick)
        }
        rafId = requestAnimationFrame(tick)
      } catch (e: any) {
        setError(e?.message ?? 'Failed to load Lua script')
      }
    }

    start()

    return () => {
      cancelled = true
      cancelAnimationFrame(rafId)
      if ((globalThis as any).matrix === matrix) {
        delete (globalThis as any).matrix
      }
    }
  }, [apiUrl, filename])

  return (
    <div className="space-y-2">
      <div className="rounded-lg border border-border bg-card p-3">
        <canvas
          ref={canvasRef}
          width={PREVIEW_SIZE * PIXEL_SCALE}
          height={PREVIEW_SIZE * PIXEL_SCALE}
          className="w-full max-w-[384px] aspect-square bg-black rounded"
        />
      </div>
      <p className="text-xs text-muted-foreground">{status}</p>
      {error && <p className="text-xs text-red-500">{error}</p>}
    </div>
  )
}

