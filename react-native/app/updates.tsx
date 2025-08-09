import * as React from 'react';
import { useEffect, useState } from 'react';
import { Dimensions, RefreshControl, ScrollView, View, ToastAndroid, Platform } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { UpdateStatus, UpdateConfig, UpdateInfo, Release } from '~/components/apiTypes/update';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import useFetch from '~/components/useFetch';
import { useUpdateInstallation } from '~/components/hooks/useUpdateInstallation';
import { CurrentStatusCard, ActionsCard, ReleasesCard, ErrorCard } from '~/components/updates';
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from '~/components/ui/alert-dialog';

export default function UpdatesScreen() {
  const updateStatus = useFetch<UpdateStatus>('/api/update/status');
  const releases = useFetch<Release[]>('/api/update/releases?per_page=5');

  const [isCheckingForUpdates, setIsCheckingForUpdates] = useState(false);
  const [manualRefresh, setManualRefresh] = useState(false);
  const [pendingUpdateVersion, setPendingUpdateVersion] = useState<string | undefined>(undefined);
  const apiUrl = useApiUrl();

  // Use the new polling-based update installation hook
  const updateInstallation = useUpdateInstallation({
    onStatusUpdate: (status) => {
      // Update the status data when we get new information during installation
      updateStatus.setData(status);
    },
    onSuccess: (version) => {
      Toast.show({
        type: 'success',
        text1: 'Update Successful!',
        text2: `Successfully updated to version ${version}`
      });
      // Refresh all data
      setRetry();
    },
    onError: (error) => {
      Toast.show({
        type: 'error',
        text1: 'Update Failed',
        text2: error
      });
      // Refresh status to get current state
      setRetry();
    }
  });

  const setRetry = () => {
    setManualRefresh(true);
    updateStatus.setRetry(Math.random());
    releases.setRetry(Math.random());
  };

  useEffect(() => {
    if (!updateStatus.isLoading && !releases.isLoading) {
      setManualRefresh(false);
    }
  }, [updateStatus.isLoading, releases.isLoading]);

  const handleCheckForUpdates = async () => {
    setIsCheckingForUpdates(true);
    try {
      const response = await fetch(apiUrl + '/api/update/check', {
        method: 'POST'
      });
      const result: UpdateInfo = await response.json();

      if (result.update_available) {
        Toast.show({
          type: 'success',
          text1: 'Update Available!',
          text2: `Version ${result.version} is available for download`
        });
      } else {
        Toast.show({
          type: 'info',
          text1: 'No Updates',
          text2: 'You are running the latest version'
        });
      }

      // Refresh status
      setRetry();
    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Check Failed',
        text2: error.message
      });
    } finally {
      setIsCheckingForUpdates(false);
    }
  };

  const handleInstallUpdate = (version?: string) => {
    setPendingUpdateVersion(version || undefined);
  };

  const confirmInstallUpdate = () => {
    updateInstallation.startInstallation(pendingUpdateVersion);
    Toast.show({
      type: 'info',
      text1: 'Update Started',
      text2: 'Monitoring installation progress...'
    });
    setPendingUpdateVersion(undefined);
  };

  const handleConfigChange = async (config: Partial<UpdateConfig>) => {
    try {
      const response = await fetch(apiUrl + '/api/update/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(config),
      });

      if (response.ok) {
        Toast.show({
          type: 'success',
          text1: 'Settings Updated',
          text2: 'Update configuration has been saved'
        });
        // Refresh status
        setRetry();
      } else {
        Toast.show({
          type: 'error',
          text1: 'Settings Failed',
          text2: 'Could not update configuration'
        });
      }
    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Settings Failed',
        text2: error.message
      });
    }
  };

  const { width } = Dimensions.get('window');
  const isWeb = width > 768;

  const isLoading = updateStatus.isLoading || releases.isLoading;
  const error = updateStatus.error ?? releases.error;

  return (
    <SafeAreaProvider>
      <SafeAreaView className="flex-1 bg-background">
        <ScrollView
          className="flex-1"
          contentContainerStyle={{
            padding: 20,
            gap: 20,
            alignItems: 'center'
          }}
          refreshControl={
            <RefreshControl
              refreshing={isLoading && manualRefresh}
              onRefresh={setRetry}
              colors={['rgb(99, 102, 241)']}
              tintColor="rgb(99, 102, 241)"
            />
          }
        >
          <View className={`w-full max-w-4xl gap-6 ${isWeb ? 'items-stretch' : 'items-center'}`}>
            {error ? (
              <ErrorCard error={error} onRetry={setRetry} />
            ) : (
              <>
                <CurrentStatusCard
                  updateStatus={updateStatus.data}
                  onConfigChange={handleConfigChange}
                  isUpdating={updateInstallation.isInstalling}
                  installProgress={updateInstallation.installProgress}
                />
                <ActionsCard
                  updateStatus={updateStatus.data}
                  isCheckingForUpdates={isCheckingForUpdates}
                  isUpdating={updateInstallation.isInstalling}
                  installProgress={updateInstallation.installProgress}
                  onCheckForUpdates={handleCheckForUpdates}
                  onInstallUpdate={() => handleInstallUpdate()}
                />
                <ReleasesCard
                  releases={releases.data}
                  updateStatus={updateStatus.data}
                  isUpdating={updateInstallation.isInstalling}
                  onInstallUpdate={handleInstallUpdate}
                />
              </>
            )}
          </View>
        </ScrollView>
        <AlertDialog open={pendingUpdateVersion !== undefined} onOpenChange={open => !open && setPendingUpdateVersion(undefined)}>
          <AlertDialogContent>
            <AlertDialogHeader>
              <AlertDialogTitle>Install Update</AlertDialogTitle>
              <AlertDialogDescription>
                Are you sure you want to install {pendingUpdateVersion ? `version ${pendingUpdateVersion}` : 'the latest update'}? The system will restart automatically.
              </AlertDialogDescription>
            </AlertDialogHeader>
            <AlertDialogFooter>
              <AlertDialogCancel onPress={() => setPendingUpdateVersion(undefined)}>
                Cancel
              </AlertDialogCancel>
              <AlertDialogAction onPress={confirmInstallUpdate}>
                Install
              </AlertDialogAction>
            </AlertDialogFooter>
          </AlertDialogContent>
        </AlertDialog>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}