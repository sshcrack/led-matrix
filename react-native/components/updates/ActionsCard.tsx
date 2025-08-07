import * as React from 'react';
import { Dimensions, View } from 'react-native';
import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Button } from '~/components/ui/button';
import { Download } from '~/lib/icons/Download';
import { RefreshCw } from '~/lib/icons/RefreshCw';
import { Settings } from '~/lib/icons/Settings';
import { UpdateStatus } from '~/components/apiTypes/update';

interface ActionsCardProps {
  updateStatus: UpdateStatus | null;
  isCheckingForUpdates: boolean;
  isUpdating: boolean;
  onCheckForUpdates: () => void;
  onInstallUpdate: () => void;
}

export function ActionsCard({ 
  updateStatus, 
  isCheckingForUpdates, 
  isUpdating, 
  onCheckForUpdates, 
  onInstallUpdate 
}: ActionsCardProps) {
  const { width } = Dimensions.get('window');
  const isWeb = width > 768;

  // Don't show actions if updates are disabled
  if (updateStatus?.status === 6) {
    return null;
  }

  return (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <CardTitle className="flex flex-row items-center gap-3">
          <View className="p-2 bg-info/10 rounded-full">
            <Settings className="text-info" width={20} height={20} />
          </View>
          <Text className="text-xl font-bold">Update Actions</Text>
        </CardTitle>
      </CardHeader>
      <CardContent className="pt-0">
        <View className={`flex flex-row gap-3 ${isWeb ? 'justify-start' : 'justify-between'}`}>
          <Button 
            variant="outline" 
            className="flex-1 max-w-48 h-16"
            onPress={onCheckForUpdates}
            disabled={isCheckingForUpdates}
          >
            <View className="flex flex-row items-center gap-2">
              <RefreshCw className="text-foreground" width={20} height={20} />
              <Text className="text-sm font-medium">
                {isCheckingForUpdates ? 'Checking...' : 'Check Now'}
              </Text>
            </View>
          </Button>
          
          {updateStatus?.update_available && (
            <Button 
              className="flex-1 max-w-48 h-16"
              onPress={onInstallUpdate}
              disabled={isUpdating}
            >
              <View className="flex flex-row items-center gap-2">
                <Download className="text-primary-foreground" width={20} height={20} />
                <Text className="text-sm font-medium text-primary-foreground">
                  {isUpdating ? 'Installing...' : 'Install Update'}
                </Text>
              </View>
            </Button>
          )}
        </View>
      </CardContent>
    </Card>
  );
}