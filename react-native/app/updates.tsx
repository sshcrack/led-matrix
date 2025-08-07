import * as React from 'react';
import { useEffect, useState } from 'react';
import { Dimensions, RefreshControl, ScrollView, View, Alert } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { UpdateStatus, UpdateConfig, UpdateInfo, Release } from '~/components/apiTypes/update';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import useFetch from '~/components/useFetch';
import { CurrentStatusCard, ActionsCard, ReleasesCard, ErrorCard } from '~/components/updates';

export default function UpdatesScreen() {
  const updateStatus = useFetch<UpdateStatus>('/api/update/status');
  const releases = useFetch<Release[]>('/api/update/releases?per_page=5');
  
  const [isUpdating, setIsUpdating] = useState(false);
  const [isCheckingForUpdates, setIsCheckingForUpdates] = useState(false);
  const [manualRefresh, setManualRefresh] = useState(false);
  const apiUrl = useApiUrl();

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

  const handleInstallUpdate = async (version?: string) => {
    Alert.alert(
      'Install Update',
      `Are you sure you want to install ${version ? `version ${version}` : 'the latest update'}? The system will restart automatically.`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Install',
          style: 'destructive',
          onPress: async () => {
            setIsUpdating(true);
            try {
              const url = version 
                ? `/api/update/install?version=${encodeURIComponent(version)}`
                : '/api/update/install';
              
              const response = await fetch(apiUrl + url, {
                method: 'POST'
              });
              
              if (response.ok) {
                Toast.show({
                  type: 'success',
                  text1: 'Update Started',
                  text2: 'The system will restart when installation is complete'
                });
              } else {
                const errorText = await response.text();
                Toast.show({
                  type: 'error',
                  text1: 'Installation Failed',
                  text2: errorText || 'Unknown error occurred'
                });
              }
            } catch (error: any) {
              Toast.show({
                type: 'error',
                text1: 'Installation Failed',
                text2: error.message
              });
            } finally {
              setIsUpdating(false);
              // Refresh status after a short delay
              setTimeout(setRetry, 1000);
            }
          }
        }
      ]
    );
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
                />
                <ActionsCard 
                  updateStatus={updateStatus.data}
                  isCheckingForUpdates={isCheckingForUpdates}
                  isUpdating={isUpdating}
                  onCheckForUpdates={handleCheckForUpdates}
                  onInstallUpdate={() => handleInstallUpdate()}
                />
                <ReleasesCard 
                  releases={releases.data}
                  updateStatus={updateStatus.data}
                  isUpdating={isUpdating}
                  onInstallUpdate={handleInstallUpdate}
                />
              </>
            )}
          </View>
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}