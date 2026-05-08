import { useEffect, useRef, useState } from 'react'
import { LuaEngine, LuaFactory } from 'wasmoon'
interface LuaPreviewProps {
  apiUrl: string
  filename: string | null
  script?: string | null
}

const PREVIEW_SIZE = 128
const PIXEL_SCALE = 3

export default function LuaPreview({ apiUrl, filename, script }: LuaPreviewProps) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [status, setStatus] = useState('Select a Lua script to preview')

  useEffect(() => {
    let rafId = 0
    let cancelled = false
    const canvas = canvasRef.current
    const ctx = canvas?.getContext('2d')
    if (!canvas || !ctx || (!filename && !script)) {
      return
    }

    const width = PREVIEW_SIZE
    const height = PREVIEW_SIZE

    let lua: LuaEngine | null = null
    const start = async () => {
      try {
        setError(null)
        setStatus(script ? `Previewing ${filename ?? 'local script'}` : `Loading ${filename}...`)
        let scriptText: string | null = null
        if (script) {
          scriptText = script
        } else if (filename != null) {
          const res = await fetch(`${apiUrl}/api/custom-assets/lua/${encodeURIComponent(filename)}/download`)
          if (!res.ok) {
            throw new Error(`Failed to fetch script (${res.status})`)
          }
          scriptText = await res.text()
        } else {
          console.warn('No script or filename provided for LuaPreview')
        }
        if (cancelled) return

        const factory = new LuaFactory()
        lua = await factory.createEngine()
        lua.global.set("log", (msg: string) => console.log(msg))
        lua.global.set("set_pixel", (x: number, y: number, r: number, g: number, b: number) => {
          ctx.fillStyle = `rgb(${Math.max(0, Math.min(255, r))}, ${Math.max(0, Math.min(255, g))}, ${Math.max(0, Math.min(255, b))})`
          ctx.fillRect(x * PIXEL_SCALE, y * PIXEL_SCALE, PIXEL_SCALE, PIXEL_SCALE)
        })

        lua.global.set("clear", () => {
          ctx.fillStyle = 'black'
          ctx.fillRect(0, 0, width * PIXEL_SCALE, height * PIXEL_SCALE)
        })

        let properties: { [key: string]: any } = {}
        lua.global.set("define_property", (name: string, type: string, defaultValue: any) => {
          properties[name] = defaultValue
        })

        lua.global.set("get_property", (name: string) => {
          return properties[name]
        })

        lua.global.set("width", width)
        lua.global.set("height", height)

        lua.global.set("time", 0)
        lua.global.set("dt", 0)

        console.log('Executing Lua script...')
        lua.doStringSync(scriptText!)
        setStatus(`Previewing ${filename ?? 'local script'}`)

        const renderFn = lua.global.get('render')
        lua.global.get("setup")()
        lua.global.get("initialize")()

        let currentTime = performance.now()
        const tick = () => {
          if (cancelled) return

          let delta = performance.now() - currentTime
          currentTime = performance.now()

          lua!.global.set("time", currentTime / 1000)
          lua!.global.set("dt", delta / 1000)

          renderFn()
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
      if(lua && !lua.global.isClosed()) {
        lua.global.close()
      }
    }
  }, [apiUrl, filename, script])

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

