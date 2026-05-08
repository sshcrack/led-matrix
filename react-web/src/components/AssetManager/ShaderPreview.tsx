import { useEffect, useState } from 'react'
import { ShadertoyRendererComponent } from '@reindernijhoff/shadertoy/react'

interface ShaderPreviewProps {
  apiUrl: string
  filename: string | null
  script?: string | null
}

const dummyShader = {
  ver: "0.1",
  info: {
    id: "preview",
    date: "0",
    viewed: 0,
    name: "Preview",
    username: "user",
    description: "",
    likes: 0,
    published: 1,
    flags: 0,
    usePreview: 0,
    tags: [],
    hasliked: 0
  },
  renderpass: [
    {
      inputs: [],
      outputs: [
        {
          id: "4dfGRr",
          channel: 0
        }
      ],
      code: "void mainImage( out vec4 fragColor, in vec2 fragCoord ) {\n    fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n}",
      name: "Image",
      description: "",
      type: "image"
    }
  ]
}

const PREVIEW_SIZE = 128
const PIXEL_SCALE = 3

export default function ShaderPreview({ apiUrl, filename, script }: ShaderPreviewProps) {
  const [error, setError] = useState<string | null>(null)
  const [status, setStatus] = useState('Select a Shader to preview')
  const [actualShader, setActualShader] = useState<any>(null)

  useEffect(() => {
    let cancelled = false

    const loadShader = async () => {
      if (!filename && !script) {
        setActualShader(null)
        setStatus('Select a Shader to preview')
        return
      }

      try {
        setError(null)
        setStatus(script ? `Previewing ${filename ?? 'local shader'}` : `Loading ${filename}...`)
        
        let scriptText: string | null = null
        if (script) {
          scriptText = script
        } else if (filename != null) {
          const res = await fetch(`${apiUrl}/api/custom-assets/shader/${encodeURIComponent(filename)}/download`)
          if (!res.ok) {
            throw new Error(`Failed to fetch shader (${res.status})`)
          }
          scriptText = await res.text()
        }

        if (cancelled) return

        if (scriptText) {
          const newShader = JSON.parse(JSON.stringify(dummyShader))
          newShader.renderpass[0].code = scriptText
          setActualShader(newShader)
          setStatus(`Previewing ${filename ?? 'local shader'}`)
        }
      } catch (e: any) {
        if (!cancelled) {
          setError(e?.message ?? 'Failed to load Shader')
          setActualShader(null)
        }
      }
    }

    loadShader()

    return () => {
      cancelled = true
    }
  }, [apiUrl, filename, script])

  return (
    <div className="space-y-2">
      <div className="rounded-lg border border-border bg-card p-3">
        <div 
          className="w-full max-w-[384px] aspect-square bg-black rounded overflow-hidden relative"
          style={{ width: PREVIEW_SIZE * PIXEL_SCALE, height: PREVIEW_SIZE * PIXEL_SCALE }}
        >
          {actualShader ? (
            <ShadertoyRendererComponent
              shader={actualShader}
              loop={true}
              pixelRatio={2}
              onReady={(renderer) => console.log('Shadertoy Ready!')}
            />
          ) : (
            <div className="w-full h-full bg-black" />
          )}
        </div>
      </div>
      <p className="text-xs text-muted-foreground">{status}</p>
      {error && <p className="text-xs text-red-500">{error}</p>}
    </div>
  )
}
