import { useEffect, useMemo, useRef, useState } from 'react'
import { Download, Trash2, Upload } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import LuaPreview from '~/components/AssetManager/LuaPreview'

type AssetType = 'lua' | 'shader'

interface CustomAsset {
  filename: string
  size: number
  last_modified: number
}

export default function AssetManager() {
  const apiUrl = useApiUrl()
  const [tab, setTab] = useState<AssetType>('lua')
  const [assets, setAssets] = useState<CustomAsset[]>([])
  const [loading, setLoading] = useState(false)
  const [selectedLua, setSelectedLua] = useState<string | null>(null)
  const fileInputRef = useRef<HTMLInputElement | null>(null)

  const endpointType = useMemo(() => (tab === 'lua' ? 'lua' : 'shader'), [tab])

  const fetchAssets = async () => {
    if (!apiUrl) return
    setLoading(true)
    try {
      const res = await fetch(`${apiUrl}/api/custom-assets/${endpointType}`)
      const data = await res.json()
      setAssets(Array.isArray(data) ? data : [])
      if (tab === 'lua' && Array.isArray(data) && data.length > 0 && !selectedLua) {
        setSelectedLua(data[0].filename)
      }
      if (tab === 'lua' && Array.isArray(data) && data.length === 0) {
        setSelectedLua(null)
      }
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    fetchAssets()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [apiUrl, endpointType])

  const onUploadClick = () => {
    fileInputRef.current?.click()
  }

  const onFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    if (!file || !apiUrl) return

    const formData = new FormData()
    formData.append('file', file)
    await fetch(`${apiUrl}/api/custom-assets/${endpointType}`, {
      method: 'POST',
      body: formData,
    })
    e.target.value = ''
    await fetchAssets()
  }

  const onDelete = async (filename: string) => {
    if (!apiUrl) return
    if (!window.confirm(`Delete ${filename}?`)) return
    await fetch(`${apiUrl}/api/custom-assets/${endpointType}/${encodeURIComponent(filename)}`, {
      method: 'DELETE',
    })
    if (tab === 'lua' && selectedLua === filename) {
      setSelectedLua(null)
    }
    await fetchAssets()
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold">Asset Manager</h1>
        <p className="text-muted-foreground text-sm mt-1">Upload, export and delete custom Lua scripts and Shadertoy shaders.</p>
      </div>

      <div className="flex gap-2">
        <Button variant={tab === 'lua' ? 'default' : 'outline'} onClick={() => setTab('lua')}>Lua Scripts</Button>
        <Button variant={tab === 'shader' ? 'default' : 'outline'} onClick={() => setTab('shader')}>Shadertoy Shaders</Button>
      </div>

      <div className="flex items-center gap-2">
        <input
          ref={fileInputRef}
          type="file"
          className="hidden"
          accept={tab === 'lua' ? '.lua' : '.frag'}
          onChange={onFileChange}
        />
        <Button onClick={onUploadClick} className="gap-2">
          <Upload className="h-4 w-4" />
          Upload {tab === 'lua' ? 'Lua Script' : 'Shader'}
        </Button>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="rounded-xl border border-border bg-card overflow-hidden">
          <div className="px-4 py-3 border-b border-border">
            <h2 className="font-medium text-sm">{tab === 'lua' ? 'Lua Scripts' : 'Shaders'} ({assets.length})</h2>
          </div>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-border text-muted-foreground">
                  <th className="text-left font-medium px-4 py-2">Filename</th>
                  <th className="text-left font-medium px-4 py-2">Size</th>
                  <th className="text-right font-medium px-4 py-2">Actions</th>
                </tr>
              </thead>
              <tbody>
                {loading ? (
                  <tr><td className="px-4 py-3 text-muted-foreground" colSpan={3}>Loading...</td></tr>
                ) : assets.length === 0 ? (
                  <tr><td className="px-4 py-3 text-muted-foreground" colSpan={3}>No files uploaded yet.</td></tr>
                ) : assets.map((asset) => (
                  <tr
                    key={asset.filename}
                    className={`border-b border-border/60 ${tab === 'lua' && selectedLua === asset.filename ? 'bg-muted/60' : ''}`}
                    onClick={() => tab === 'lua' && setSelectedLua(asset.filename)}
                  >
                    <td className="px-4 py-2">{asset.filename}</td>
                    <td className="px-4 py-2 text-muted-foreground">{Math.max(1, Math.round(asset.size / 1024))} KB</td>
                    <td className="px-4 py-2">
                      <div className="flex justify-end gap-2">
                        <Button variant="outline" size="sm" asChild className="gap-1">
                          <a href={`${apiUrl}/api/custom-assets/${endpointType}/${encodeURIComponent(asset.filename)}/download`}>
                            <Download className="h-3.5 w-3.5" />
                            Export
                          </a>
                        </Button>
                        <Button variant="destructive" size="sm" className="gap-1" onClick={(e) => { e.stopPropagation(); onDelete(asset.filename) }}>
                          <Trash2 className="h-3.5 w-3.5" />
                          Delete
                        </Button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        <div className="rounded-xl border border-border bg-card p-4">
          {tab === 'lua' ? (
            <LuaPreview apiUrl={apiUrl ?? ''} filename={selectedLua} />
          ) : (
            <p className="text-sm text-muted-foreground">Select and export shaders from the table on the left.</p>
          )}
        </div>
      </div>
    </div>
  )
}

