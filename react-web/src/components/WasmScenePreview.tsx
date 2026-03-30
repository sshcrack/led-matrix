/**
 * WasmScenePreview
 *
 * Renders a live scene preview using the compiled WASM module running inside
 * a dedicated Web Worker. Falls back to the static GIF endpoint when:
 *   - The WASM module is not available (not yet built / 404)
 *   - The scene is not in the WASM registry (needs_desktop, etc.)
 *   - An error occurs during WASM initialisation
 *
 * Props:
 *   sceneName    – scene identifier used by wasm_scene_create()
 *   hasPreview   – whether a GIF fallback is available (/scene_preview?name=…)
 *   apiUrl       – base URL for the matrix REST API (used by GIF fallback)
 *   properties   – current property values as a Record<string, unknown>
 *   width        – internal render width  (default 128)
 *   height       – internal render height (default 128)
 *   className    – optional CSS class applied to the wrapper <div>
 */

import { useEffect, useRef, useState, useCallback } from 'react'
import { ImageOff } from 'lucide-react'

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

type WorkerInMessage =
  | { type: 'init';           sceneName: string; width: number; height: number }
  | { type: 'set_properties'; json: string }
  | { type: 'stop' }

type WorkerOutMessage =
  | { type: 'frame';   imageData: ImageData }
  | { type: 'error';   message: string }
  | { type: 'stopped' }

// ---------------------------------------------------------------------------
// Determine at module load time which scenes have a WASM preview.
// This list is derived from the ExampleScenes plugin compiled into
// scene_preview.wasm (v1). Expand as more plugins are added to the WASM build.
// ---------------------------------------------------------------------------
const WASM_SUPPORTED_SCENES = new Set([
  'color_pulse',
  'property_demo',
  'rendering_demo',
])

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

interface WasmScenePreviewProps {
  sceneName: string
  hasPreview?: boolean
  apiUrl: string
  properties?: Record<string, unknown>
  width?: number
  height?: number
  className?: string
}

type PreviewMode = 'wasm' | 'gif' | 'none'

export default function WasmScenePreview({
  sceneName,
  hasPreview,
  apiUrl,
  properties,
  width  = 128,
  height = 128,
  className,
}: WasmScenePreviewProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const workerRef = useRef<Worker | null>(null)
  const [mode, setMode]     = useState<PreviewMode>('none')
  const [gifError, setGifError] = useState(false)

  // Determine which mode to use
  const preferWasm = WASM_SUPPORTED_SCENES.has(sceneName)

  // -------------------------------------------------------------------------
  // Worker lifecycle
  // -------------------------------------------------------------------------

  const stopWorker = useCallback(() => {
    if (workerRef.current) {
      workerRef.current.postMessage({ type: 'stop' } satisfies WorkerInMessage)
      workerRef.current.terminate()
      workerRef.current = null
    }
  }, [])

  const startWorker = useCallback(() => {
    stopWorker()
    setMode('none')

    const worker = new Worker(
      new URL('../workers/scenePreview.worker.ts', import.meta.url),
      { type: 'module' },
    )
    workerRef.current = worker

    worker.onmessage = (e: MessageEvent<WorkerOutMessage>) => {
      const msg = e.data
      if (msg.type === 'frame') {
        const canvas = canvasRef.current
        if (!canvas) return
        const ctx = canvas.getContext('2d')
        if (!ctx) return
        ctx.putImageData(msg.imageData, 0, 0)
        if (mode !== 'wasm') setMode('wasm')
      } else if (msg.type === 'error') {
        console.warn('[WasmScenePreview] worker error:', msg.message)
        stopWorker()
        // Fall back to GIF
        setMode(hasPreview ? 'gif' : 'none')
      }
    }

    worker.onerror = () => {
      stopWorker()
      setMode(hasPreview ? 'gif' : 'none')
    }

    worker.postMessage({
      type: 'init',
      sceneName,
      width,
      height,
    } satisfies WorkerInMessage)
  }, [sceneName, width, height, hasPreview, mode, stopWorker])

  // Start / restart when sceneName changes
  useEffect(() => {
    if (preferWasm) {
      startWorker()
    } else {
      stopWorker()
      setMode(hasPreview ? 'gif' : 'none')
    }
    return () => stopWorker()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [sceneName, preferWasm])

  // Push property updates to the running worker
  useEffect(() => {
    if (!workerRef.current || !properties) return
    workerRef.current.postMessage({
      type: 'set_properties',
      json: JSON.stringify(properties),
    } satisfies WorkerInMessage)
  }, [properties])

  // -------------------------------------------------------------------------
  // Render
  // -------------------------------------------------------------------------

  const gifSrc = `${apiUrl}/scene_preview?name=${encodeURIComponent(sceneName)}`

  if (mode === 'wasm') {
    return (
      <div className={className}>
        <canvas
          ref={canvasRef}
          width={width}
          height={height}
          style={{ width: '100%', height: '100%', objectFit: 'contain', imageRendering: 'pixelated' }}
        />
      </div>
    )
  }

  if (mode === 'gif' && !gifError) {
    return (
      <div className={className}>
        <img
          src={gifSrc}
          alt={`${sceneName} preview`}
          style={{ width: '100%', height: '100%', objectFit: 'contain' }}
          onError={() => setGifError(true)}
        />
      </div>
    )
  }

  // No preview available: show a placeholder, and keep canvas hidden so
  // the ref stays valid while the worker is still initialising.
  return (
    <div className={className} style={{ display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
      {preferWasm && (
        // Canvas remains in the DOM while the WASM worker is loading so the
        // worker can paint into it as soon as the first frame arrives.
        <canvas ref={canvasRef} width={width} height={height} style={{ display: 'none' }} />
      )}
      {!preferWasm && (
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 8 }}>
          <ImageOff style={{ width: 24, height: 24, opacity: 0.4 }} />
          <span style={{ fontSize: 12, opacity: 0.6 }}>No preview</span>
        </div>
      )}
    </div>
  )
}
