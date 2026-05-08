import { useEffect, useMemo, useRef, useState } from 'react'
import { Download, Trash2, Upload } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription, DialogFooter } from '~/components/ui/dialog'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import LuaPreview from '~/components/AssetManager/LuaPreview'
import ShaderPreview from '~/components/AssetManager/ShaderPreview'



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
  const [selectedShader, setSelectedShader] = useState<string | null>(null)
  const fileInputRef = useRef<HTMLInputElement | null>(null)
  const [pendingFile, setPendingFile] = useState<File | null>(null)
  const [previewScript, setPreviewScript] = useState<string | null>(null)
  const [showUploadDialog, setShowUploadDialog] = useState(false)

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
      if (tab === 'shader' && Array.isArray(data) && data.length > 0 && !selectedShader) {
        setSelectedShader(data[0].filename)
      }
      if (tab === 'shader' && Array.isArray(data) && data.length === 0) {
        setSelectedShader(null)
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
    
    // Store the selected file and show a preview dialog. Only upload after confirmation.
    setPendingFile(file)
    try {
      const text = await file.text()
      setPreviewScript(text)
      setShowUploadDialog(true)
    } catch (err) {
      // fallback: clear selection
      setPendingFile(null)
      e.target.value = ''
    }
  }

  const handleDialogOpenChange = (open: boolean) => {
    setShowUploadDialog(open)
    if (!open) {
      setPendingFile(null)
      setPreviewScript(null)
      if (fileInputRef.current) fileInputRef.current.value = ''
    }
  }

  const confirmUpload = async () => {
    if (!pendingFile || !apiUrl) return
    const formData = new FormData()
    formData.append('file', pendingFile)
    await fetch(`${apiUrl}/api/custom-assets/${endpointType}`, {
      method: 'POST',
      body: formData,
    })
    handleDialogOpenChange(false)
    // cleanup handled by handleDialogOpenChange when dialog closes
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
    if (tab === 'shader' && selectedShader === filename) {
      setSelectedShader(null)
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
                    className={`border-b border-border/60 ${
                      tab === 'lua' ? (selectedLua === asset.filename ? 'bg-muted/60' : '') 
                                    : (selectedShader === asset.filename ? 'bg-muted/60' : '')
                    }`}
                    onClick={() => {
                      if (tab === 'lua') setSelectedLua(asset.filename)
                      else setSelectedShader(asset.filename)
                    }}
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
            <ShaderPreview apiUrl={apiUrl ?? ''} filename={selectedShader} />
          )}
        </div>
      </div>

      <Dialog open={showUploadDialog} onOpenChange={handleDialogOpenChange}>
        <DialogContent className="max-w-2xl">
          <DialogHeader>
            <DialogTitle>Preview Upload</DialogTitle>
            <DialogDescription>{pendingFile ? pendingFile.name : 'Preview the script before uploading.'}</DialogDescription>
          </DialogHeader>
          <div className="p-2">
            {tab === 'lua' ? (
              <LuaPreview apiUrl={apiUrl ?? ''} filename={pendingFile?.name ?? null} script={previewScript} />
            ) : (
              <ShaderPreview apiUrl={apiUrl ?? ''} filename={pendingFile?.name ?? null} script={previewScript} />
            )}
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => handleDialogOpenChange(false)}>Cancel</Button>
            <Button onClick={confirmUpload} className="ml-2">Upload</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </div>
  )
}

