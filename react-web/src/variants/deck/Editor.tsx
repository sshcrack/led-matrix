import { useNavigate, useParams } from 'react-router-dom'
import { ChevronLeft, Play } from 'lucide-react'
import { useMatrix } from '~/core/matrixContext'
import { PresetEditor } from '~/core/ui/PresetEditor'

export default function DeckEditor() {
  const { preset_id } = useParams()
  const id = decodeURIComponent(preset_id ?? '')
  const navigate = useNavigate()
  const { activate, presets } = useMatrix()
  return <div>
    <div className="sticky top-0 z-30 -mx-4 mb-3 flex items-center gap-2 bg-background/90 px-3 py-3 backdrop-blur">
      <button onClick={() => navigate('/presets')} aria-label="Back" className="grid h-10 w-10 place-items-center rounded-full bg-secondary"><ChevronLeft className="h-5 w-5" /></button>
      <span className="flex-1 truncate font-bold">{presets?.[id]?.display_name ?? 'Edit preset'}</span>
      <button onClick={() => void activate(id)} className="flex items-center gap-1.5 rounded-full bg-primary px-4 py-2 text-sm font-bold text-primary-foreground"><Play className="h-4 w-4" />Play</button>
    </div>
    <PresetEditor presetId={id} layout="stack" />
  </div>
}
