import { useCallback, useEffect, useRef, useState } from 'react'
import { Check, Link as LinkIcon, Loader2, Play, Plus, Search, X } from 'lucide-react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { propertyDescription, propertyLabel } from '~/components/modify-preset/propertyUi'
import type { Property } from '~/apiTypes/list_scenes'

interface VideoResult {
  id: string
  url: string
  title: string
  channel: string
  duration: number
  thumbnail: string
}

interface SearchSnapshot {
  state: 'idle' | 'searching' | 'done' | 'error'
  query: string
  error: string
  items: VideoResult[]
}

interface Props {
  property: Property<string[]>
  value: string[]
  onChange: (value: string[]) => void
}

const TITLE_CACHE_KEY = 'led-matrix:video-titles'

// Titles of videos added through search, so the playlist can show names
// instead of raw links. Purely a display convenience; the saved value is URLs.
function readTitles(): Record<string, string> {
  try {
    return JSON.parse(localStorage.getItem(TITLE_CACHE_KEY) ?? '{}')
  } catch {
    return {}
  }
}

function rememberTitle(id: string, title: string) {
  try {
    localStorage.setItem(TITLE_CACHE_KEY, JSON.stringify({ ...readTitles(), [id]: title }))
  } catch {
    // storage unavailable (private window); the UI falls back to the link
  }
}

function videoId(url: string): string | null {
  try {
    const u = new URL(url)
    if (u.hostname === 'youtu.be') return u.pathname.slice(1) || null
    return u.searchParams.get('v')
  } catch {
    return null
  }
}

function formatDuration(seconds: number): string {
  if (!seconds) return ''
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  return h > 0
    ? `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`
    : `${m}:${String(s).padStart(2, '0')}`
}

async function errorMessage(res: Response): Promise<string> {
  try {
    const body = await res.json()
    if (typeof body?.error === 'string') return body.error
    if (typeof body?.message === 'string') return body.message
  } catch {
    // not JSON
  }
  return `Request failed (${res.status})`
}

export default function VideoPlaylistProperty({ property, value, onChange }: Props) {
  const apiUrl = useApiUrl()
  const list = Array.isArray(value) ? value : []

  const [query, setQuery] = useState('')
  const [snapshot, setSnapshot] = useState<SearchSnapshot | null>(null)
  const [requestError, setRequestError] = useState<string | null>(null)
  const [link, setLink] = useState('')
  const [titles, setTitles] = useState(readTitles)
  const [playing, setPlaying] = useState<string | null>(null)
  const pollTimer = useRef<ReturnType<typeof setTimeout> | undefined>(undefined)
  // Bumped per search so a slow poll of an older search cannot overwrite newer results.
  const searchSeq = useRef(0)

  useEffect(() => () => clearTimeout(pollTimer.current), [])

  const poll = useCallback((seq: number) => {
    const tick = async () => {
      if (!apiUrl) return
      try {
        const res = await fetch(`${apiUrl}/video/search_results`)
        if (!res.ok) throw new Error(await errorMessage(res))
        const snap = (await res.json()) as SearchSnapshot
        if (seq !== searchSeq.current) return
        setSnapshot(snap)
        if (snap.state === 'searching') pollTimer.current = setTimeout(tick, 700)
      } catch (e) {
        if (seq !== searchSeq.current) return
        setRequestError(e instanceof Error ? e.message : 'Search failed')
        setSnapshot(null)
      }
    }
    return tick()
  }, [apiUrl])

  const search = async () => {
    const q = query.trim()
    if (!q || !apiUrl) return
    clearTimeout(pollTimer.current)
    const seq = ++searchSeq.current
    setRequestError(null)
    setSnapshot({ state: 'searching', query: q, error: '', items: [] })
    try {
      const res = await fetch(`${apiUrl}/video/search?q=${encodeURIComponent(q)}`)
      if (!res.ok) throw new Error(await errorMessage(res))
      pollTimer.current = setTimeout(() => poll(seq), 700)
    } catch (e) {
      setRequestError(e instanceof Error ? e.message : 'Search failed')
      setSnapshot(null)
    }
  }

  const add = (item: VideoResult) => {
    rememberTitle(item.id, item.title)
    setTitles(readTitles())
    if (!list.includes(item.url)) onChange([...list, item.url])
  }

  const addLink = () => {
    const v = link.trim()
    if (!/^https?:\/\//.test(v)) return
    if (!list.includes(v)) onChange([...list, v])
    setLink('')
  }

  const remove = (url: string) => onChange(list.filter((u) => u !== url))

  const playNow = async (url: string) => {
    if (!apiUrl) return
    setPlaying(url)
    try {
      const res = await fetch(`${apiUrl}/video/play?url=${encodeURIComponent(url)}`, { method: 'POST' })
      if (!res.ok) throw new Error(await errorMessage(res))
      setRequestError(null)
    } catch (e) {
      setRequestError(e instanceof Error ? e.message : 'Could not start playback')
    }
    setTimeout(() => setPlaying((p) => (p === url ? null : p)), 1500)
  }

  const searching = snapshot?.state === 'searching'
  const items = snapshot?.state === 'done' ? snapshot.items : []

  return (
    <div className="space-y-3">
      <div className="space-y-1.5">
        <Label>{propertyLabel(property)}</Label>
        {propertyDescription(property) && (
          <p className="text-xs leading-relaxed text-muted-foreground">{propertyDescription(property)}</p>
        )}
      </div>

      {/* Search */}
      <div className="flex gap-2">
        <Input
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && search()}
          placeholder="Search YouTube..."
        />
        <Button type="button" variant="outline" onClick={search} disabled={!query.trim() || searching} className="gap-2">
          {searching ? <Loader2 className="h-4 w-4 animate-spin" /> : <Search className="h-4 w-4" />}
          Search
        </Button>
      </div>

      {(requestError || snapshot?.state === 'error') && (
        <p className="text-xs text-destructive">{requestError ?? snapshot?.error}</p>
      )}
      {snapshot?.state === 'done' && items.length === 0 && (
        <p className="text-xs text-muted-foreground">No results for “{snapshot.query}”.</p>
      )}

      {items.length > 0 && (
        <ul className="max-h-80 divide-y divide-border/60 overflow-y-auto rounded-lg border border-border/70">
          {items.map((item) => {
            const added = list.includes(item.url)
            return (
              <li key={item.id} className="flex items-center gap-2 p-2 sm:gap-3">
                <img src={item.thumbnail} alt="" loading="lazy" className="h-9 w-14 shrink-0 rounded object-cover bg-muted sm:h-12 sm:w-20" />
                <div className="min-w-0 flex-1">
                  <p className="truncate text-sm font-medium" title={item.title}>{item.title}</p>
                  <p className="truncate text-xs text-muted-foreground">
                    {item.channel}{item.channel && item.duration ? ' · ' : ''}{formatDuration(item.duration)}
                  </p>
                </div>
                <Button type="button" size="icon" variant="ghost" title="Play now" onClick={() => playNow(item.url)}>
                  {playing === item.url ? <Check className="h-4 w-4" /> : <Play className="h-4 w-4" />}
                </Button>
                <Button type="button" size="sm" variant={added ? 'secondary' : 'outline'} disabled={added} onClick={() => add(item)} className="gap-1 px-2 sm:px-3" aria-label={added ? 'Added' : 'Add'}>
                  {added ? <Check className="h-4 w-4" /> : <Plus className="h-4 w-4" />}
                  <span className="hidden sm:inline">{added ? 'Added' : 'Add'}</span>
                </Button>
              </li>
            )
          })}
        </ul>
      )}

      {/* Playlist */}
      {list.length > 0 ? (
        <ul className="divide-y divide-border/60 rounded-lg border border-border/70">
          {list.map((url) => {
            const id = videoId(url)
            const title = (id && titles[id]) || url
            return (
              <li key={url} className="flex items-center gap-2 p-2 sm:gap-3">
                {id ? (
                  <img src={`https://i.ytimg.com/vi/${id}/default.jpg`} alt="" loading="lazy" className="h-8 w-12 shrink-0 rounded object-cover bg-muted sm:h-9 sm:w-14" />
                ) : (
                  <div className="flex h-8 w-12 sm:h-9 sm:w-14 shrink-0 items-center justify-center rounded bg-muted"><LinkIcon className="h-4 w-4 text-muted-foreground" /></div>
                )}
                <p className="min-w-0 flex-1 truncate text-sm" title={url}>{title}</p>
                <Button type="button" size="icon" variant="ghost" title="Play now" onClick={() => playNow(url)}>
                  {playing === url ? <Check className="h-4 w-4" /> : <Play className="h-4 w-4" />}
                </Button>
                <Button type="button" size="icon" variant="ghost" title="Remove" onClick={() => remove(url)} className="hover:text-destructive">
                  <X className="h-4 w-4" />
                </Button>
              </li>
            )
          })}
        </ul>
      ) : (
        <p className="text-xs italic text-muted-foreground">Playlist is empty. Search above to add videos.</p>
      )}

      {/* Manual link */}
      <div className="flex gap-2">
        <Input
          value={link}
          onChange={(e) => setLink(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && addLink()}
          placeholder="Or paste a video link..."
        />
        <Button type="button" variant="outline" size="icon" onClick={addLink} title="Add link">
          <Plus className="h-4 w-4" />
        </Button>
      </div>
    </div>
  )
}
