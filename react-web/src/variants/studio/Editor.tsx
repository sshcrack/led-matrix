import { useNavigate, useParams } from 'react-router-dom'
import { ChevronLeft, Play } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { useMatrix } from '~/core/matrixContext'
import { PresetEditor } from '~/core/ui/PresetEditor'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'

export default function StudioEditor() {
  const { preset_id } = useParams()
  const id = decodeURIComponent(preset_id ?? '')
  const navigate = useNavigate()
  const { activate, apiUrl, status } = useMatrix()
  const playing = status?.current === id && status.operation_mode === 'manual'
  return <div className="mx-auto max-w-6xl px-4 pb-24 pt-6 sm:px-8">
    <div className="mb-4 flex items-center gap-2">
      <Button variant="ghost" size="sm" onClick={() => navigate('/')}><ChevronLeft className="h-4 w-4" />Library</Button>
      <span className="flex-1" />
      <Button size="sm" variant={playing ? 'outline' : 'default'} onClick={() => void activate(id)}><Play className="h-4 w-4" />{playing ? 'Playing' : 'Play to preview'}</Button>
    </div>
    <div className="grid gap-5 xl:grid-cols-[minmax(0,1fr)_22rem]">
      <PresetEditor presetId={id} layout="split" />
      {playing && <div className="hidden xl:block"><div className="sticky top-6"><LiveMatrixPreview apiUrl={apiUrl ?? ''} label="Live preview" /></div></div>}
    </div>
  </div>
}
