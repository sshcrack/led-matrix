import * as React from 'react';
import { View } from 'react-native';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Button } from '~/components/ui/button';
import { Badge } from '~/components/ui/badge';
import { Download } from '~/lib/icons/Download';
import { Release, UpdateStatus } from '~/components/apiTypes/update';

interface ReleasesCardProps {
  releases: Release[] | null;
  updateStatus: UpdateStatus | null;
  isUpdating: boolean;
  onInstallUpdate: (version: string) => void;
}

export function ReleasesCard({ releases, updateStatus, isUpdating, onInstallUpdate }: ReleasesCardProps) {
  if (!releases || releases.length === 0) return null;

  // Don't show releases if updates are disabled
  if (updateStatus?.status === 6) {
    return null;
  }

  return (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <CardTitle className="flex flex-row items-center gap-3">
          <View className="p-2 bg-success/10 rounded-full">
            <Download className="text-success" width={20} height={20} />
          </View>
          <Text className="text-xl font-bold">Recent Releases</Text>
        </CardTitle>
        <CardDescription className="text-base">
          Available versions for download
        </CardDescription>
      </CardHeader>
      <CardContent className="pt-0 space-y-3">
        {releases.map((release, index) => (
          <View
            key={release.version}
            className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl"
          >
            <View className="flex-1">
              <View className="flex flex-row items-center gap-2">
                <Text className="text-lg font-semibold">{release.version}</Text>
                {release.is_prerelease ? (
                  <Badge variant="secondary" className="text-xs">Pre-release</Badge>
                ) : null}
                {index === 0 ? (
                  <Badge variant="default" className="text-xs">Latest</Badge>
                ) : null}
              </View>
              <Text className="text-sm text-muted-foreground mt-1">
                {release.name || 'No release notes'}
              </Text>
              <Text className="text-xs text-muted-foreground">
                {new Date(release.published_at).toLocaleDateString()}
              </Text>
            </View>

            {release.download_url ? (
              <Button
                variant="outline"
                size="sm"
                onPress={() => onInstallUpdate(release.version)}
                disabled={isUpdating}
              >
                <View className="flex flex-row items-center gap-1">
                  <Download width={16} height={16} />
                  <Text className="text-xs">Install</Text>
                </View>
              </Button>
            ) : null}
          </View>
        ))}
      </CardContent>
    </Card>
  );
}