import { Power } from 'lucide-react'
import { Card, CardContent } from '~/components/ui/card'
import { Switch } from '~/components/ui/switch'
import { StatusIndicator } from '~/components/ui/status-indicator'
import { Skeleton } from '~/components/ui/skeleton'
import type { Status } from '~/apiTypes/status'

interface StatusCardProps {
  status: Status | null
  isLoading: boolean
  onToggle: (enabled: boolean) => void
}

export default function StatusCard({ status, isLoading, onToggle }: StatusCardProps) {
  if (isLoading) {
    return (
      <Card>
        <CardContent className="p-6">
          <div className="flex items-center justify-between">
            <div className="space-y-2">
              <Skeleton className="h-4 w-24" />
              <Skeleton className="h-6 w-32" />
            </div>
            <Skeleton className="h-8 w-14 rounded-full" />
          </div>
        </CardContent>
      </Card>
    )
  }

  const isOn = status ? !status.turned_off : false
  const currentPreset = status?.current || 'None'

  return (
    <Card>
      <CardContent className="p-6">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className={`w-10 h-10 rounded-xl flex items-center justify-center ${isOn ? 'bg-success/10' : 'bg-muted'}`}>
              <Power className={`h-5 w-5 ${isOn ? 'text-success' : 'text-muted-foreground'}`} />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <StatusIndicator status={isOn ? 'online' : 'offline'} />
                <span className="text-sm font-medium text-muted-foreground">
                  {isOn ? 'Matrix On' : 'Matrix Off'}
                </span>
              </div>
              <p className="font-semibold mt-0.5">
                {isOn ? currentPreset : 'Standby'}
              </p>
            </div>
          </div>
          <Switch
            checked={isOn}
            onCheckedChange={onToggle}
            aria-label="Toggle matrix power"
          />
        </div>
      </CardContent>
    </Card>
  )
}
