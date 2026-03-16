import { Tag, Calendar, ExternalLink } from 'lucide-react'
import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card'
import { Badge } from '~/components/ui/badge'
import { Skeleton } from '~/components/ui/skeleton'
import type { Release } from '~/apiTypes/update'

interface ReleasesCardProps {
  releases: Release[] | null
  isLoading: boolean
  currentVersion?: string
}

export function ReleasesCard({ releases, isLoading, currentVersion }: ReleasesCardProps) {
  if (isLoading) {
    return (
      <Card>
        <CardContent className="p-6 space-y-3">
          {[1, 2, 3].map(i => <Skeleton key={i} className="h-16 w-full" />)}
        </CardContent>
      </Card>
    )
  }

  if (!releases?.length) return null

  return (
    <Card>
      <CardHeader className="pb-3">
        <CardTitle className="text-base">Recent Releases</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        {releases.map((release) => {
          const isCurrent = release.version === currentVersion
          const date = new Date(release.published_at).toLocaleDateString()
          return (
            <div key={release.version} className={`p-3 rounded-lg border ${isCurrent ? 'border-primary/40 bg-primary/5' : 'border-border'}`}>
              <div className="flex items-start justify-between gap-2">
                <div className="flex items-center gap-2 flex-wrap">
                  <div className="flex items-center gap-1.5">
                    <Tag className="h-3.5 w-3.5 text-muted-foreground" />
                    <span className="font-mono font-medium text-sm">{release.version}</span>
                  </div>
                  {isCurrent && <Badge variant="secondary" className="text-xs">Current</Badge>}
                  {release.is_prerelease && <Badge variant="warning" className="text-xs">Pre-release</Badge>}
                </div>
                <div className="flex items-center gap-1 text-xs text-muted-foreground shrink-0">
                  <Calendar className="h-3 w-3" />
                  {date}
                </div>
              </div>
              {release.name && release.name !== release.version && (
                <p className="text-sm text-muted-foreground mt-1">{release.name}</p>
              )}
            </div>
          )
        })}
      </CardContent>
    </Card>
  )
}

export default ReleasesCard
