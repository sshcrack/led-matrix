import useFetch from '~/useFetch'
import { useUpdateInstallation } from '~/components/hooks/useUpdateInstallation'
import CurrentStatusCard from '~/components/updates/CurrentStatusCard'
import ReleasesCard from '~/components/updates/ReleasesCard'
import type { UpdateStatus, Release } from '~/apiTypes/update'

export default function Updates() {
  const { data: status, isLoading: statusLoading, setRetry: retryStatus, setData: setStatus } = useFetch<UpdateStatus>('/api/update/status')
  const { data: releases, isLoading: releasesLoading } = useFetch<Release[]>('/api/update/releases?per_page=5')

  const { installation, checkForUpdates, installUpdate } = useUpdateInstallation({
    onStatus: setStatus,
    onRefresh: () => retryStatus(r => r + 1),
  })

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold">Firmware Updates</h1>
        <p className="text-muted-foreground text-sm mt-1">Manage controller firmware</p>
      </div>

      <CurrentStatusCard
        status={status}
        isLoading={statusLoading && !status}
        installation={installation}
        onCheck={checkForUpdates}
        onInstall={installUpdate}
      />

      <ReleasesCard
        releases={releases}
        isLoading={releasesLoading}
        currentVersion={status?.current_version}
      />
    </div>
  )
}
