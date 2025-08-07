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
                  text1: 'Update Started',
                  text2: 'The update is being installed. The system will restart soon.'
                });
              } else {
                throw new Error('Installation failed');
              }
            } catch (error: any) {
              Toast.show({
                type: 'error',
                text1: 'Installation Failed',
                text2: error.message
              });
            } finally {
              setIsUpdating(false);
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
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(config)
      });

      if (response.ok) {
        Toast.show({
          type: 'success',
          text1: 'Settings Updated',
          text2: 'Update configuration has been saved'
        });
        setRetry();
      } else {
        throw new Error('Configuration update failed');
      }
    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Update Failed',
        text2: error.message
      });
    }
  };

  const { width } = Dimensions.get('window');
  const isWeb = width > 768;

  const isLoading = updateStatus.isLoading || releases.isLoading;
  const error = updateStatus.error ?? releases.error;

  const getStatusColor = (status: number) => {
    switch (status) {
      case 0: return 'default'; // IDLE
      case 1: return 'secondary'; // CHECKING
      case 2: return 'secondary'; // DOWNLOADING
      case 3: return 'secondary'; // INSTALLING
      case 4: return 'destructive'; // ERROR
      case 5: return 'default'; // SUCCESS
      default: return 'default';
    }
  };

  const getStatusIcon = (status: number) => {
    switch (status) {
      case 0: return <CheckCircle className="text-success" width={16} height={16} />;
      case 1: case 2: case 3: return <Clock className="text-info" width={16} height={16} />;
      case 4: return <AlertCircle className="text-destructive" width={16} height={16} />;
      case 5: return <CheckCircle className="text-success" width={16} height={16} />;
      default: return <CheckCircle className="text-muted" width={16} height={16} />;
    }
  };

  const CurrentStatusCard = () => (
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
        {updateStatus.data && (
          <>
            {/* Current Version */}
            <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
              <View>
                <Text className="text-lg font-semibold">Current Version</Text>
                <Text className="text-sm text-muted-foreground">
                  {updateStatus.data.current_version || 'Unknown'}
                </Text>
              </View>
              <Badge variant={updateStatus.data.update_available ? 'secondary' : 'default'}>
                {updateStatus.data.update_available ? 'Update Available' : 'Up to Date'}
              </Badge>
            </View>

            {/* Update Status */}
            <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
              <View className="flex flex-row items-center gap-3">
                {getStatusIcon(updateStatus.data.status)}
                <View>
                  <Text className="text-lg font-semibold">
                    {UpdateStatusNames[updateStatus.data.status as keyof typeof UpdateStatusNames]}
                  </Text>
                  {updateStatus.data.error_message && (
                    <Text className="text-sm text-destructive">
                      {updateStatus.data.error_message}
                    </Text>
                  )}
                </View>
              </View>
              <Badge variant={getStatusColor(updateStatus.data.status)}>
                Status
              </Badge>
            </View>

            {/* Auto Update Setting */}
            <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
              <View>
                <Text className="text-lg font-semibold">Automatic Updates</Text>
                <Text className="text-sm text-muted-foreground">
                  Check every {updateStatus.data.check_interval_hours} hours
                </Text>
              </View>
              <Switch
                checked={updateStatus.data.auto_update_enabled}
                onCheckedChange={(enabled) => handleConfigChange({ auto_update_enabled: enabled })}
                nativeID='auto-update-enabled'
              />
            </View>
          </>
        )}
      </CardContent>
    </Card>
  );

  const ActionsCard = () => (
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
            onPress={handleCheckForUpdates}
            disabled={isCheckingForUpdates}
          >
            <View className="flex flex-row items-center gap-2">
              <RefreshCw className="text-foreground" width={20} height={20} />
              <Text className="text-sm font-medium">
                {isCheckingForUpdates ? 'Checking...' : 'Check Now'}
              </Text>
            </View>
          </Button>
          
          {updateStatus.data?.update_available && (
            <Button 
              className="flex-1 max-w-48 h-16"
              onPress={() => handleInstallUpdate()}
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

  const ReleasesCard = () => {
    if (!releases.data || releases.data.length === 0) return null;

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
          {releases.data.map((release, index) => (
            <View 
              key={release.version} 
              className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl"
            >
              <View className="flex-1">
                <View className="flex flex-row items-center gap-2">
                  <Text className="text-lg font-semibold">{release.version}</Text>
                  {release.is_prerelease && (
                    <Badge variant="secondary" className="text-xs">Pre-release</Badge>
                  )}
                  {index === 0 && (
                    <Badge variant="default" className="text-xs">Latest</Badge>
                  )}
                </View>
                <Text className="text-sm text-muted-foreground mt-1">
                  {release.name || 'No release notes'}
                </Text>
                <Text className="text-xs text-muted-foreground">
                  {new Date(release.published_at).toLocaleDateString()}
                </Text>
              </View>
              
              {release.download_url && (
                <Button
                  variant="outline"
                  size="sm"
                  onPress={() => handleInstallUpdate(release.version)}
                  disabled={isUpdating}
                >
                  <View className="flex flex-row items-center gap-1">
                    <Download width={16} height={16} />
                    <Text className="text-xs">Install</Text>
                  </View>
                </Button>
              )}
            </View>
          ))}
        </CardContent>
      </Card>
    );
  };

  const ErrorCard = () => (
    <Card className="w-full shadow-lg border-destructive/20 bg-destructive/5">
      <CardContent className="pt-6">
        <View className="flex items-center gap-4">
          <View className="p-3 bg-destructive/10 rounded-full">
            <AlertCircle className="text-destructive" width={24} height={24} />
          </View>
          <Text className="text-lg font-semibold text-destructive">
            Connection Error
          </Text>
          <Text className="text-center text-muted-foreground">
            {error?.message || "Unable to connect to update service"}
          </Text>
          <Button onPress={setRetry} className="mt-2">
            <Text>Retry Connection</Text>
          </Button>
        </View>
      </CardContent>
    </Card>
  );

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
              <ErrorCard />
            ) : (
              <>
                <CurrentStatusCard />
                <ActionsCard />
                <ReleasesCard />
              </>
            )}
          </View>
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}