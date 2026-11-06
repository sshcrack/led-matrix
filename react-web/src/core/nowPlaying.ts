import { useMatrix } from './matrixContext'
import { presetThumbNames } from './scenes'

/** What the matrix is showing right now, in words and in scene names. */
export function useNowPlaying() {
  const { status, current, scenes } = useMatrix()
  const automatic = status?.operation_mode === 'automatic'
  const label = status?.turned_off ? 'Off' : automatic ? 'Automatic Director' : (current?.display_name || status?.current || '—')
  const names = current && scenes ? presetThumbNames(current, scenes, 3) : []
  return { label, automatic, off: !!status?.turned_off, names, firstScene: current?.scenes[0]?.type ?? null }
}
