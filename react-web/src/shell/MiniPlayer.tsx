import { ModeSwitch } from '~/core/ui/controls'
import { SceneThumb } from '~/core/ui/SceneThumb'
import { useNowPlaying } from '~/core/nowPlaying'

/** Phone-only: what's playing, always visible above the tab bar, so you never have to scroll back to the preview. */
export default function MiniPlayer() {
  const now = useNowPlaying()
  return <div className="flex items-center gap-3 rounded-2xl border border-border bg-card/95 p-2 pr-2.5 shadow-lg backdrop-blur-xl">
    <SceneThumb name={now.firstScene ?? ''} className="h-10 w-10 shrink-0 rounded-xl" />
    <div className="min-w-0 flex-1">
      <div className="font-mono text-[9px] uppercase tracking-widest text-muted-foreground">{now.off ? 'Display off' : now.automatic ? 'Director' : 'Playing'}</div>
      <div className="truncate text-sm font-semibold leading-tight">{now.label}</div>
    </div>
    <ModeSwitch className="shrink-0 [&_button]:px-2.5" />
  </div>
}
