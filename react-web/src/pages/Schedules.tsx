import { useState } from 'react'
import { Plus, Trash2, Clock, Calendar } from 'lucide-react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Switch } from '~/components/ui/switch'
import { Badge } from '~/components/ui/badge'
import { Skeleton } from '~/components/ui/skeleton'
import {
  Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription
} from '~/components/ui/dialog'
import {
  AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent,
  AlertDialogDescription, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle,
} from '~/components/ui/alert-dialog'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import type { ListPresets } from '~/apiTypes/list_presets'

/** Matches the server's ConfigData::Schedule struct */
interface Schedule {
  id: string
  name: string
  preset_id: string
  start_hour: number    // 0-23
  start_minute: number  // 0-59
  end_hour: number      // 0-23
  end_minute: number    // 0-59
  days_of_week: number[] // 0=Sun … 6=Sat
  enabled: boolean
}

type NewSchedule = Omit<Schedule, 'id'>

interface SchedulingStatus {
  enabled: boolean
  active_preset: string
}

const DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']

function pad(n: number) {
  return String(n).padStart(2, '0')
}

function formatTime(hour: number, minute: number) {
  return `${pad(hour)}:${pad(minute)}`
}

const DEFAULT_NEW: NewSchedule = {
  name: '',
  preset_id: '',
  start_hour: 8,
  start_minute: 0,
  end_hour: 17,
  end_minute: 0,
  days_of_week: [1, 2, 3, 4, 5],
  enabled: true,
}

export default function Schedules() {
  const apiUrl = useApiUrl()
  const { data: schedules, isLoading, setRetry } = useFetch<Record<string, Schedule>>('/schedules')
  const { data: schedulingStatus, setData: setSchedulingStatus } = useFetch<SchedulingStatus>('/scheduling_status')
  const { data: presets } = useFetch<ListPresets>('/list_presets')
  const [showCreate, setShowCreate] = useState(false)
  const [deleteId, setDeleteId] = useState<string | null>(null)
  const [newSchedule, setNewSchedule] = useState<NewSchedule>(DEFAULT_NEW)

  const handleToggleScheduling = async (enabled: boolean) => {
    if (!apiUrl) return
    try {
      await fetch(`${apiUrl}/scheduling_status`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled }),
      })
      setSchedulingStatus(prev => prev ? { ...prev, enabled } : null)
      toast.success(enabled ? 'Scheduling enabled' : 'Scheduling disabled')
    } catch {
      toast.error('Failed to update scheduling status')
    }
  }

  const handleCreate = async () => {
    if (!apiUrl || !newSchedule.preset_id || newSchedule.days_of_week.length === 0) {
      toast.error('Please fill in all fields and select at least one day')
      return
    }
    try {
      await fetch(`${apiUrl}/schedule`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(newSchedule),
      })
      toast.success('Schedule created')
      setShowCreate(false)
      setNewSchedule(DEFAULT_NEW)
      setRetry(r => r + 1)
    } catch {
      toast.error('Failed to create schedule')
    }
  }

  const handleDelete = async (id: string) => {
    if (!apiUrl) return
    try {
      await fetch(`${apiUrl}/schedule?id=${encodeURIComponent(id)}`, { method: 'DELETE' })
      toast.success('Schedule deleted')
      setRetry(r => r + 1)
    } catch {
      toast.error('Failed to delete schedule')
    }
  }

  const toggleDay = (day: number) => {
    setNewSchedule(prev => ({
      ...prev,
      days_of_week: prev.days_of_week.includes(day)
        ? prev.days_of_week.filter(d => d !== day)
        : [...prev.days_of_week, day],
    }))
  }

  const presetOptions = presets
    ? Object.entries(presets).map(([id, preset]) => ({ id, label: preset.display_name ?? id }))
    : []

  const presetDisplayById = presets
    ? Object.fromEntries(Object.entries(presets).map(([id, preset]) => [id, preset.display_name ?? id]))
    : {}

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold">Schedules</h1>
        <p className="text-muted-foreground text-sm mt-1">Automate preset switching by time</p>
      </div>

      {/* Scheduling toggle */}
      <Card>
        <CardContent className="p-6">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className="w-10 h-10 rounded-xl bg-primary/10 flex items-center justify-center">
                <Clock className="h-5 w-5 text-primary" />
              </div>
              <div>
                <p className="font-medium">Auto Scheduling</p>
                <p className="text-sm text-muted-foreground">Switch presets on a schedule</p>
              </div>
            </div>
            <Switch
              checked={schedulingStatus?.enabled ?? false}
              onCheckedChange={handleToggleScheduling}
            />
          </div>
        </CardContent>
      </Card>

      {/* Schedules list */}
      <div className="space-y-3">
        <div className="flex items-center justify-between">
          <h2 className="font-semibold">Schedules</h2>
          <Button size="sm" onClick={() => setShowCreate(true)} className="gap-1.5">
            <Plus className="h-4 w-4" />
            Add
          </Button>
        </div>

        {isLoading ? (
          <div className="space-y-2">
            {[1, 2].map(i => <Skeleton key={i} className="h-24 w-full rounded-xl" />)}
          </div>
        ) : schedules && Object.keys(schedules).length > 0 ? (
          <div className="space-y-2">
            {Object.entries(schedules).map(([id, schedule]) => (
              <Card key={id}>
                <CardContent className="p-4">
                  <div className="flex items-start justify-between">
                    <div className="space-y-1.5">
                      <div className="flex items-center gap-2">
                        <Calendar className="h-4 w-4 text-muted-foreground" />
                        <span className="font-medium">{schedule.name || schedule.preset_id}</span>
                      </div>
                      <p className="text-sm text-muted-foreground">{presetDisplayById[schedule.preset_id] ?? schedule.preset_id}</p>
                      <p className="text-lg font-semibold">
                        {formatTime(schedule.start_hour, schedule.start_minute)}
                        {' – '}
                        {formatTime(schedule.end_hour, schedule.end_minute)}
                      </p>
                      <div className="flex gap-1 flex-wrap">
                        {DAYS.map((day, i) => (
                          <Badge
                            key={day}
                            variant={schedule.days_of_week.includes(i) ? 'default' : 'outline'}
                            className="text-xs px-1.5 py-0"
                          >
                            {day}
                          </Badge>
                        ))}
                      </div>
                    </div>
                    <Button
                      variant="ghost"
                      size="icon"
                      className="h-8 w-8 text-muted-foreground hover:text-destructive"
                      onClick={() => setDeleteId(id)}
                    >
                      <Trash2 className="h-4 w-4" />
                    </Button>
                  </div>
                </CardContent>
              </Card>
            ))}
          </div>
        ) : (
          <Card>
            <CardContent className="p-8 text-center">
              <Calendar className="h-8 w-8 text-muted-foreground mx-auto mb-3" />
              <p className="text-muted-foreground text-sm">No schedules yet</p>
              <Button size="sm" variant="outline" className="mt-3" onClick={() => setShowCreate(true)}>
                Create first schedule
              </Button>
            </CardContent>
          </Card>
        )}
      </div>

      {/* Create dialog */}
      <Dialog open={showCreate} onOpenChange={setShowCreate}>
        <DialogContent className="max-w-md">
          <DialogHeader>
            <DialogTitle>Create Schedule</DialogTitle>
            <DialogDescription>Automatically switch to a preset during a time window.</DialogDescription>
          </DialogHeader>
          <div className="space-y-4">
            <div className="space-y-2">
              <Label>Name</Label>
              <Input
                placeholder="e.g. Morning"
                value={newSchedule.name}
                onChange={e => setNewSchedule(p => ({ ...p, name: e.target.value }))}
              />
            </div>
            <div className="space-y-2">
              <Label>Preset</Label>
              <Select value={newSchedule.preset_id} onValueChange={v => setNewSchedule(p => ({ ...p, preset_id: v }))}>
                <SelectTrigger>
                  <SelectValue placeholder="Select preset..." />
                </SelectTrigger>
                <SelectContent>
                  {presetOptions.map(preset => (
                    <SelectItem key={preset.id} value={preset.id}>{preset.label}</SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>
            <div className="grid grid-cols-2 gap-3">
              <div className="space-y-2">
                <Label>Start time</Label>
                <Input
                  type="time"
                  value={formatTime(newSchedule.start_hour, newSchedule.start_minute)}
                  onChange={e => {
                    const [h, m] = e.target.value.split(':').map(Number)
                    setNewSchedule(p => ({ ...p, start_hour: h ?? 0, start_minute: m ?? 0 }))
                  }}
                />
              </div>
              <div className="space-y-2">
                <Label>End time</Label>
                <Input
                  type="time"
                  value={formatTime(newSchedule.end_hour, newSchedule.end_minute)}
                  onChange={e => {
                    const [h, m] = e.target.value.split(':').map(Number)
                    setNewSchedule(p => ({ ...p, end_hour: h ?? 0, end_minute: m ?? 0 }))
                  }}
                />
              </div>
            </div>
            <div className="space-y-2">
              <Label>Days</Label>
              <div className="flex gap-1.5 flex-wrap">
                {DAYS.map((day, i) => (
                  <button
                    key={day}
                    type="button"
                    onClick={() => toggleDay(i)}
                    className={`px-2.5 py-1 rounded-md text-xs font-medium transition-colors ${
                      newSchedule.days_of_week.includes(i)
                        ? 'bg-primary text-primary-foreground'
                        : 'bg-secondary text-secondary-foreground hover:bg-secondary/80'
                    }`}
                  >
                    {day}
                  </button>
                ))}
              </div>
            </div>
            <div className="flex items-center gap-2">
              <Switch
                id="enabled"
                checked={newSchedule.enabled}
                onCheckedChange={v => setNewSchedule(p => ({ ...p, enabled: v }))}
              />
              <Label htmlFor="enabled">Enabled</Label>
            </div>
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setShowCreate(false)}>Cancel</Button>
            <Button onClick={handleCreate} disabled={!newSchedule.preset_id || newSchedule.days_of_week.length === 0}>
              Create
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>

      {/* Delete confirmation */}
      <AlertDialog open={!!deleteId} onOpenChange={() => setDeleteId(null)}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Delete Schedule</AlertDialogTitle>
            <AlertDialogDescription>This schedule will be permanently deleted.</AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction
              className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
              onClick={() => { if (deleteId) { handleDelete(deleteId); setDeleteId(null) } }}
            >
              Delete
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>
    </div>
  )
}
