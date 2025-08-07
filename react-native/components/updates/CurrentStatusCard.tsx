import * as React from 'react';
import { View } from 'react-native';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Badge } from '~/components/ui/badge';
import { Switch } from '~/components/ui/switch';
import { Download } from '~/lib/icons/Download';
import { CheckCircle } from '~/lib/icons/CheckCircle';
import { AlertCircle } from '~/lib/icons/AlertCircle';
import { Clock } from '~/lib/icons/Clock';
import { UpdateStatus, UpdateStatusNames } from '~/components/apiTypes/update';

interface CurrentStatusCardProps {
  updateStatus: UpdateStatus | null;
  onConfigChange: (config: { auto_update_enabled: boolean }) => void;
}

export function CurrentStatusCard({ updateStatus, onConfigChange }: CurrentStatusCardProps) {
  const getStatusColor = (status: number) => {
    switch (status) {
      case 0: return 'default'; // IDLE
      case 1: return 'secondary'; // CHECKING
      case 2: return 'secondary'; // DOWNLOADING
      case 3: return 'secondary'; // INSTALLING
      case 4: return 'destructive'; // ERROR
      case 5: return 'default'; // SUCCESS
      case 6: return 'destructive'; // DISABLED
      default: return 'default';
    }
  };

  const getStatusIcon = (status: number) => {
    switch (status) {
      case 0: return <CheckCircle className="text-success" width={16} height={16} />;
      case 1: case 2: case 3: return <Clock className="text-info" width={16} height={16} />;
      case 4: return <AlertCircle className="text-destructive" width={16} height={16} />;
      case 5: return <CheckCircle className="text-success" width={16} height={16} />;
      case 6: return <AlertCircle className="text-destructive" width={16} height={16} />;
      default: return <CheckCircle className="text-muted" width={16} height={16} />;
    }
  };

  return (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <CardTitle className="flex flex-row items-center gap-3">
          <View className="p-2 bg-primary/10 rounded-full">
            <Download className="text-primary" width={24} height={24} />
          </View>
          <Text className="text-2xl font-bold">System Updates</Text>
        </CardTitle>
        <CardDescription className="text-base">
          Manage automatic updates and system versions
        </CardDescription>
      </CardHeader>
      <CardContent className="pt-0 space-y-4">
        {updateStatus && (
          <>
            {/* Current Version */}
            <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
              <View>
                <Text className="text-lg font-semibold">Current Version</Text>
                <Text className="text-sm text-muted-foreground">
                  {updateStatus.current_version || 'Unknown'}
                </Text>
              </View>
              <Badge variant={updateStatus.update_available ? 'secondary' : 'default'}>
                {updateStatus.update_available ? 'Update Available' : 'Up to Date'}
              </Badge>
            </View>

            {/* Update Status */}
            <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
              <View className="flex flex-row items-center gap-3">
                {getStatusIcon(updateStatus.status)}
                <View>
                  <Text className="text-lg font-semibold">
                    {UpdateStatusNames[updateStatus.status as keyof typeof UpdateStatusNames]}
                  </Text>
                  {updateStatus.error_message && (
                    <Text className="text-sm text-destructive">
                      {updateStatus.error_message}
                    </Text>
                  )}
                </View>
              </View>
              <Badge variant={getStatusColor(updateStatus.status)}>
                Status
              </Badge>
            </View>

            {/* Auto Update Setting */}
            {updateStatus.status !== 6 && ( // Don't show auto-update toggle if disabled
              <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
                <View>
                  <Text className="text-lg font-semibold">Automatic Updates</Text>
                  <Text className="text-sm text-muted-foreground">
                    Check every {updateStatus.check_interval_hours} hours
                  </Text>
                </View>
                <Switch
                  checked={updateStatus.auto_update_enabled}
                  onCheckedChange={(enabled) => onConfigChange({ auto_update_enabled: enabled })}
                  nativeID='auto-update-enabled'
                />
              </View>
            )}
            
            {/* Manual Update Required Notice */}
            {updateStatus.status === 6 && (
              <View className="flex flex-row items-center justify-center p-4 bg-yellow-500/20 rounded-xl">
                <View className="flex flex-row items-center gap-3">
                  <AlertCircle className="text-yellow-600" width={20} height={20} />
                  <Text className="text-sm text-yellow-800 dark:text-yellow-200">
                    Automatic updates are disabled. Manual updates required.
                  </Text>
                </View>
              </View>
            )}
          </>
        )}
      </CardContent>
    </Card>
  );
}