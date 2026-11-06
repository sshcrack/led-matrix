import { useNavigate, useParams } from 'react-router-dom'
import { ChevronLeft, Play } from 'lucide-react'
import { useMatrix } from '~/core/matrixContext'
import { PresetEditor } from '~/core/ui/PresetEditor'

export default function AmbientEditor() {
  const { preset_id } = useParams()
  const id = decodeURIComponent(preset_id ?? '')
  const navigate = useNavigate()
  const { activate } = useMatrix()
  return <div className="mx-auto max-w-2xl pt-2">
    <div className="mb-3 flex items-center justify-between">
      <button onClick={() => navigate('/')} className="flex items-center gap-1 text-sm text-muted-foreground hover:text-foreground"><ChevronLeft className="h-4 w-4" />Back</button>
      <button onClick={() => void activate(id)} className="flex items-center gap-1.5 rounded-full bg-primary px-4 py-2 text-sm font-medium text-primary-foreground shadow-lg shadow-primary/30"><Play className="h-4 w-4" />Play</button>
    </div>
    <PresetEditor presetId={id} layout="stack" />
  </div>
}
