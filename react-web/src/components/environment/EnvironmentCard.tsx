import { useState } from 'react'
import { CloudSun, MapPin, Search } from 'lucide-react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Switch } from '~/components/ui/switch'
import { Skeleton } from '~/components/ui/skeleton'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'

/** Matches the daemon's GET /environment (Environment::Service::status). */
export interface EnvironmentOverrides {
  time_of_day: string
  weather: string
  season: string
  /** "auto", "off" or a comma separated list of occasion ids */
  occasions: string
}

export interface EnvironmentStatus {
  enabled: boolean
  auto_locate: boolean
  latitude: number | null
  longitude: number | null
  label: string
  detected_label: string
  overrides: EnvironmentOverrides
  location: { latitude: number, longitude: number, label: string, source: string } | null
  error: string | null
  occasions: { id: string, label: string }[]
  active_occasions: string[]
  conditions: {
    sun_elevation_deg: number
    season: string
    local_hour: number
    weather: string | null
    temperature_c?: number
    cloud_cover?: number
    wind_speed_ms?: number
    weather_age_seconds?: number
  } | null
}

type Patch = Partial<Pick<EnvironmentStatus, 'enabled' | 'auto_locate' | 'latitude' | 'longitude' | 'label'>> & {
  overrides?: Partial<EnvironmentOverrides>
}

interface Place { name: string, latitude: number, longitude: number }

const TIME_OPTIONS = [['auto', 'Real time'], ['dawn', 'Dawn'], ['day', 'Day'], ['dusk', 'Dusk'], ['night', 'Night']]
const WEATHER_OPTIONS = [['auto', 'Real weather'], ['clear', 'Clear'], ['cloudy', 'Cloudy'], ['rain', 'Rain'], ['snow', 'Snow'], ['fog', 'Fog'], ['storm', 'Storm']]
const SEASON_OPTIONS = [['auto', 'Calendar'], ['spring', 'Spring'], ['summer', 'Summer'], ['autumn', 'Autumn'], ['winter', 'Winter']]

const WEATHER_LABELS: Record<string, string> = {
  clear: 'clear', partly_cloudy: 'partly cloudy', overcast: 'overcast', fog: 'fog', drizzle: 'drizzle',
  rain: 'rain', snow: 'snow', storm: 'thunderstorm',
}

function describe(status: EnvironmentStatus) {
  const c = status.conditions
  if (!status.enabled) return 'Off: scenes use their designed look (overrides still apply)'
  if (!c) return status.location ? 'Waiting for the first update…' : 'Looking up the location…'
  const hour = Math.floor(c.local_hour)
  const minute = Math.round((c.local_hour - hour) * 60) % 60
  const parts = [`${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`]
  parts.push(c.sun_elevation_deg > 0 ? `sun ${Math.round(c.sun_elevation_deg)}° up` : 'sun down')
  if (c.weather) parts.push(WEATHER_LABELS[c.weather] ?? c.weather)
  if (c.temperature_c !== undefined) parts.push(`${Math.round(c.temperature_c)} °C`)
  parts.push(c.season)
  return parts.join(' · ')
}

function OverrideSelect({ id, label, value, options, disabled, onChange }: {
  id: string
  label: string
  value: string
  options: string[][]
  disabled: boolean
  onChange: (value: string) => void
}) {
  return (
    <div className="space-y-1.5">
      <Label htmlFor={id}>{label}</Label>
      <Select value={value} onValueChange={onChange} disabled={disabled}>
        <SelectTrigger id={id} className="h-10 text-base sm:text-sm"><SelectValue /></SelectTrigger>
        <SelectContent>
          {options.map(([v, l]) => <SelectItem key={v} value={v}>{l}</SelectItem>)}
        </SelectContent>
      </Select>
    </div>
  )
}

function EnvironmentForm({ status, onSaved }: { status: EnvironmentStatus, onSaved: (next: EnvironmentStatus) => void }) {
  const apiUrl = useApiUrl()
  const [saving, setSaving] = useState(false)
  const [query, setQuery] = useState('')
  const [places, setPlaces] = useState<Place[] | null>(null)
  const [searching, setSearching] = useState(false)

  const save = async (patch: Patch, message?: string) => {
    if (!apiUrl) return
    setSaving(true)
    try {
      const res = await fetch(`${apiUrl}/environment`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(patch),
      })
      if (!res.ok) throw new Error(await res.text())
      onSaved((await res.json()) as EnvironmentStatus)
      if (message) toast.success(message)
    } catch {
      toast.error('Failed to save real-world settings')
    } finally {
      setSaving(false)
    }
  }

  const search = async () => {
    const q = query.trim()
    if (!apiUrl || !q) return
    setSearching(true)
    try {
      const res = await fetch(`${apiUrl}/environment/geocode?q=${encodeURIComponent(q)}`)
      if (!res.ok) throw new Error(await res.text())
      setPlaces((await res.json()) as Place[])
    } catch {
      toast.error('Place search failed')
      setPlaces(null)
    } finally {
      setSearching(false)
    }
  }

  const manual = status.latitude !== null && status.longitude !== null
  const occasionMode = status.overrides.occasions === 'auto' || status.overrides.occasions === 'off' ? status.overrides.occasions : 'custom'
  const chosen = occasionMode === 'custom' ? status.overrides.occasions.split(',') : []
  const toggleOccasion = (id: string) => {
    const next = chosen.includes(id) ? chosen.filter((o) => o !== id) : [...chosen, id]
    void save({ overrides: { occasions: next.length ? next.join(',') : 'off' } })
  }

  return (
    <CardContent className="p-6 space-y-5">
      <div className="flex items-center justify-between gap-3">
        <div className="flex items-center gap-3 min-w-0">
          <div className="w-10 h-10 shrink-0 rounded-xl bg-primary/10 flex items-center justify-center">
            <CloudSun className="h-5 w-5 text-primary" />
          </div>
          <div className="min-w-0">
            <p className="font-medium">Real-world scenery</p>
            <p className="text-sm text-muted-foreground">{describe(status)}</p>
          </div>
        </div>
        <Switch
          checked={status.enabled}
          disabled={saving}
          aria-label="Follow the real time, weather and season"
          onCheckedChange={(enabled) => void save({ enabled })}
        />
      </div>
      <p className="text-sm text-muted-foreground">
        Outdoor scenes follow the time of day, weather and season where the matrix is. A scene can still be pinned to
        a fixed look in its Environment settings.
      </p>
      {status.error && status.enabled && <p className="text-sm text-destructive">{status.error}</p>}

      {status.enabled && (
        <div className="space-y-3">
          <div className="flex items-center gap-2 text-sm">
            <MapPin className="h-4 w-4 text-muted-foreground" />
            <span className="truncate">
              {status.location
                ? `${status.location.label || `${status.location.latitude.toFixed(2)}, ${status.location.longitude.toFixed(2)}`}${status.location.source === 'ip' ? ' (detected from IP)' : ''}`
                : 'No location yet'}
            </span>
            {manual && (
              <Button variant="ghost" size="sm" className="ml-auto" disabled={saving}
                onClick={() => void save({ latitude: null, longitude: null, label: '' }, 'Using the detected location')}>
                Use detected
              </Button>
            )}
          </div>
          {!manual && (
            <div className="flex items-center justify-between gap-3">
              <Label htmlFor="env-auto-locate" className="font-normal">Detect location from the IP address</Label>
              <Switch id="env-auto-locate" checked={status.auto_locate} disabled={saving}
                onCheckedChange={(auto_locate) => void save({ auto_locate })} />
            </div>
          )}
          <form className="flex gap-2" onSubmit={(e) => { e.preventDefault(); void search() }}>
            <Input
              aria-label="Search for a place"
              placeholder="Set location: search a city"
              className="h-10 text-base sm:text-sm"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
            />
            <Button type="submit" variant="secondary" className="h-10" disabled={searching || !query.trim()}>
              <Search className="h-4 w-4" />
              <span className="sr-only sm:not-sr-only sm:ml-1">Search</span>
            </Button>
          </form>
          {places && (
            <div className="rounded-lg border divide-y">
              {places.length === 0 && <p className="p-3 text-sm text-muted-foreground">No places found.</p>}
              {places.map((p) => (
                <button
                  key={`${p.latitude},${p.longitude}`}
                  type="button"
                  className="block w-full min-h-10 px-3 py-2 text-left text-sm hover:bg-muted"
                  onClick={() => {
                    setPlaces(null)
                    setQuery('')
                    void save({ latitude: p.latitude, longitude: p.longitude, label: p.name }, `Location set to ${p.name}`)
                  }}
                >
                  {p.name}
                  <span className="ml-2 text-muted-foreground">{p.latitude.toFixed(2)}, {p.longitude.toFixed(2)}</span>
                </button>
              ))}
            </div>
          )}
        </div>
      )}

      <div className="space-y-3">
        <div>
          <p className="text-sm font-medium">Override for every scene</p>
          <p className="text-sm text-muted-foreground">Make it snow because it feels like winter, or keep it night all day.</p>
        </div>
        <div className="grid grid-cols-1 gap-3 sm:grid-cols-3">
          <OverrideSelect id="env-time" label="Time of day" value={status.overrides.time_of_day} options={TIME_OPTIONS}
            disabled={saving} onChange={(time_of_day) => void save({ overrides: { time_of_day } })} />
          <OverrideSelect id="env-weather" label="Weather" value={status.overrides.weather} options={WEATHER_OPTIONS}
            disabled={saving} onChange={(weather) => void save({ overrides: { weather } })} />
          <OverrideSelect id="env-season" label="Season" value={status.overrides.season} options={SEASON_OPTIONS}
            disabled={saving} onChange={(season) => void save({ overrides: { season } })} />
        </div>
        <div className="space-y-2">
          <OverrideSelect id="env-occasions" label="Holiday decorations" value={occasionMode}
            options={[['auto', 'By the calendar'], ['off', 'None'], ['custom', 'Always these…']]}
            disabled={saving}
            onChange={(mode) => void save({
              overrides: { occasions: mode === 'custom' ? (status.occasions[0]?.id ?? 'off') : mode },
            })} />
          {occasionMode === 'custom' && (
            <div className="flex flex-wrap gap-2">
              {status.occasions.map((o) => (
                <Button key={o.id} type="button" size="sm" className="h-10" disabled={saving}
                  variant={chosen.includes(o.id) ? 'default' : 'outline'} onClick={() => toggleOccasion(o.id)}>
                  {o.label}
                </Button>
              ))}
            </div>
          )}
          {status.active_occasions.length > 0 && (
            <p className="text-sm text-muted-foreground">
              Showing now: {status.active_occasions.map((id) => status.occasions.find((o) => o.id === id)?.label ?? id).join(', ')}
            </p>
          )}
        </div>
      </div>
    </CardContent>
  )
}

export default function EnvironmentCard() {
  const { data, error, isLoading, setData } = useFetch<EnvironmentStatus>('/environment')
  return (
    <Card>
      {isLoading && !data ? (
        <CardContent className="p-6"><Skeleton className="h-10 w-full" /></CardContent>
      ) : error || !data || typeof data.enabled !== 'boolean' ? (
        <CardContent className="p-6 text-sm text-muted-foreground">Could not load the real-world scenery settings.</CardContent>
      ) : (
        <EnvironmentForm status={data} onSaved={setData} />
      )}
    </Card>
  )
}
