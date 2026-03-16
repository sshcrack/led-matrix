import { Download, RefreshCw, Server, CheckCircle2, AlertCircle, Loader2 } from 'lucide-react'
import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { Progress } from '~/components/ui/progress'
import { Skeleton } from '~/components/ui/skeleton'
import { UpdateStatusNames, type UpdateStatus } from '~/apiTypes/update'
import type { InstallState } from '~/components/hooks/useUpdateInstallation'

interface CurrentStatusCardProps {
  status: UpdateStatus | null
  isLoading: boolean
  installState: InstallState
  onCheck: () => void
  onInstall: () => void
}

export function CurrentStatusCard({ status, isLoading, installState, onCheck, onInstall }: CurrentStatusCardProps) {
  if (isLoading) {
    return (
      <Card>
        <CardContent className="p-6 space-y-3">
          <Skeleton className="h-4 w-32" />
          <Skeleton className="h-8 w-48" />
          <Skeleton className="h-4 w-full" />
        </CardContent>
      </Card>
    )
  }

  if (!status) return null

  const isInProgress = status.status === 1 || status.status === 2 || status.status === 3
  const progressMap: Record<number, number> = { 1: 15, 2: 50, 3: 85 }
  const progress = progressMap[status.status] || 0

  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="text-base flex items-center gap-2">
          <Server className="h-4 w-4" />
          Current Status
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="flex items-center justify-between">
          <div>
            <p className="text-sm text-muted-foreground">Current version</p>
            <p className="font-semibold font-mono">{status.current_version || 'Unknown'}</p>
          </div>
          {status.update_available && (
            <div className="text-right">
              <p className="text-sm text-muted-foreground">Latest</p>
              <p className="font-semibold font-mono text-primary">{status.latest_version}</p>
            </div>
          )}
        </div>

        <div className="flex items-center gap-2">
          {status.status === 4 ? (
            <AlertCircle className="h-4 w-4 text-destructive" />
          ) : status.status === 5 ? (
            <CheckCircle2 className="h-4 w-4 text-success" />
          ) : isInProgress ? (
            <Loader2 className="h-4 w-4 animate-spin text-primary" />
          ) : null}
          <Badge
            variant={
              status.status === 4 ? 'destructive' :
              status.status === 5 ? 'success' :
              isInProgress ? 'info' : 'secondary'
            }
          >
            {UpdateStatusNames[status.status as keyof typeof UpdateStatusNames] || 'Unknown'}
          </Badge>
        </div>

        {isInProgress && (
          <Progress value={progress} />
        )}

        {status.error_message && (
          <p className="text-sm text-destructive">{status.error_message}</p>
        )}

        <div className="flex gap-2 flex-wrap">
          <Button
            variant="outline"
            size="sm"
            onClick={onCheck}
            disabled={isInProgress || installState === 'installing'}
            className="gap-2"
          >
            <RefreshCw className="h-4 w-4" />
            Check for updates
          </Button>
          {status.update_available && (
            <Button
              size="sm"
              onClick={onInstall}
              disabled={isInProgress || installState === 'installing'}
              className="gap-2"
            >
              <Download className="h-4 w-4" />
              Install update
            </Button>
          )}
        </div>
      </CardContent>
    </Card>
  )
}

export default CurrentStatusCard
