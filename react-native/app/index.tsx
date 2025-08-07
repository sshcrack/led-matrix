import { Link } from 'expo-router';
import * as React from 'react';
import { useEffect, useState } from 'react';
import { Dimensions, RefreshControl, ScrollView, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { ListPresets } from '~/components/apiTypes/list_presets';
import { Status } from '~/components/apiTypes/status';
import { UpdateStatus } from '~/components/apiTypes/update';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import AddPresetButton from '~/components/home/AddPresetButton';
import Preset from '~/components/home/Preset';
import { Button } from '~/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '~/components/ui/card';
import { StatusIndicator } from '~/components/ui/status-indicator';
import { Switch } from '~/components/ui/switch';
import { Text } from '~/components/ui/text';
import { Badge } from '~/components/ui/badge';
import useFetch from '~/components/useFetch';
import { Activity } from '~/lib/icons/Activity';
import { Calendar } from '~/lib/icons/Calendar';
import { Download } from '~/lib/icons/Download';
import { Power } from '~/lib/icons/Power';
import { Settings } from '~/lib/icons/Settings';

export default function Screen() {
  const presets = useFetch<ListPresets>(`/list_presets`);
  const status = useFetch<Status>(`/status`);
  const updateStatus = useFetch<UpdateStatus>(`/api/update/status`);

  const [settingStatus, setSettingStatus] = useState(false);
  const [turnedOn, setTurnedOn] = useState<null | boolean>(null);
  const [manualRefresh, setManualRefresh] = useState(false);
  const apiUrl = useApiUrl();

  const setRetry = () => {
    setTurnedOn(null);
    setSettingStatus(false);
    setManualRefresh(true);
    presets.setRetry(Math.random());
    status.setRetry(Math.random());
    updateStatus.setRetry(Math.random());
  };

  React.useEffect(() => {
    if (!settingStatus) return;

    fetch(apiUrl + `/set_enabled?enabled=${turnedOn ? "true" : "false"}`)
      .catch(e => {
        Toast.show({
          type: "error",
          text1: "Error setting status",
          text2: e.message
        });
      })
      .finally(() => setSettingStatus(false));
  }, [turnedOn, settingStatus]);

  React.useEffect(() => {
    if (status.data && turnedOn === null) {
      setTurnedOn(!status.data.turned_off);
    }
  }, [status.data]);

  const isLoading = presets.isLoading || status.isLoading || updateStatus.isLoading;
  const error = presets.error ?? status.error ?? updateStatus.error;

  useEffect(() => {
    if (!isLoading) setManualRefresh(false);
  }, [isLoading]);

  const { width } = Dimensions.get('window');
  const isWeb = width > 768;

  const StatusCard = () => (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <CardTitle className="flex flex-row items-center gap-3">
          <View className="p-2 bg-primary/10 rounded-full">
            <Power className="text-primary" width={24} height={24} />
          </View>
          <Text className="text-2xl font-bold">Matrix Control</Text>
        </CardTitle>
        <CardDescription className="text-base">
          Manage your LED matrix display system
        </CardDescription>
      </CardHeader>
      <CardContent className="pt-0">
        <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
          <View className="flex flex-row items-center gap-3">
            <StatusIndicator
              status={turnedOn ? 'active' : 'inactive'}
              size="lg"
            />
            <View>
              <Text className="text-lg font-semibold">
                {settingStatus ? "Updating..." : turnedOn ? "Active" : "Inactive"}
              </Text>
              <Text className="text-sm text-muted-foreground">
                Matrix display is {turnedOn ? "enabled" : "disabled"}
              </Text>
            </View>
          </View>
          <Switch
            disabled={turnedOn === null || settingStatus}
            checked={turnedOn ?? false}
            onCheckedChange={v => {
              setSettingStatus(true);
              setTurnedOn(v);
            }}
            nativeID='led-matrix-status'
          />
        </View>
      </CardContent>
    </Card>
  );

  const QuickActionsCard = () => (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <CardTitle className="flex flex-row items-center gap-3">
          <View className="p-2 bg-info/10 rounded-full">
            <Activity className="text-info" width={20} height={20} />
          </View>
          <Text className="text-xl font-bold">Quick Actions</Text>
        </CardTitle>
      </CardHeader>
      <CardContent className="pt-0">
        <View className={`flex flex-row gap-3 ${isWeb ? 'justify-start' : 'justify-between'}`}>
          <Link href="/schedules" asChild>
            <Button variant="outline" className="flex-1 max-w-48 h-16">
              <View className="flex flex-row items-center gap-2">
                <Calendar className="text-foreground" width={20} height={20} />
                <Text className="text-sm font-medium">Schedules</Text>
              </View>
            </Button>
          </Link>
          <Link href="/updates" asChild>
            <Button variant="outline" className="flex-1 max-w-48 h-16">
              <View className="flex flex-row items-center gap-2">
                <View className="relative">
                  <Download className="text-foreground" width={20} height={20} />
                  {updateStatus.data?.update_available && (
                    <View className="absolute -top-1 -right-1 w-3 h-3 bg-red-500 rounded-full" />
                  )}
                </View>
                <View className="flex flex-col items-start">
                  <Text className="text-sm font-medium">Updates</Text>
                  {updateStatus.data?.update_available && (
                    <Badge variant="destructive" className="text-xs px-1 py-0 h-4">
                      New
                    </Badge>
                  )}
                </View>
              </View>
            </Button>
          </Link>
          <Button variant="outline" className="flex-1 max-w-48 h-16" onPress={setRetry}>
            <View className="flex flex-row items-center gap-2">
              <Settings className="text-foreground" width={20} height={20} />
              <Text className="text-sm font-medium">Refresh</Text>
            </View>
          </Button>
        </View>
      </CardContent>
    </Card>
  );

  const PresetsSection = () => {
    if (!status.data || !presets.data) return null;

    const presetEntries = Object.entries(presets.data);
    const activePreset = status.data?.current;

    return (
      <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
        <CardHeader className="pb-4">
          <View className='w-full flex flex-row items-center justify-between'>
            <View className="flex flex-row items-center gap-3">
              <View className="p-2 bg-success/10 rounded-full">
                <Settings className="text-success" width={20} height={20} />
              </View>
              <Text className="text-xl font-bold">Presets</Text>
            </View>
            <View className='flex flex-col items-end'>
              <Text className="text-sm text-muted-foreground">
                {presetEntries.length} available
              </Text>
            </View>
          </View>
          <CardDescription className="text-base pt-10">
            Select and manage your LED matrix presets
          </CardDescription>
        </CardHeader>
        <CardContent className="pt-0">
          <View className={`flex flex-row flex-wrap gap-4 ${isWeb ? 'justify-start' : 'justify-center'}`}>
            {presetEntries.map(([key, preset]) => (
              <Preset
                key={key}
                preset={preset}
                name={key}
                isActive={activePreset === key}
                setStatusRefresh={() => status.setRetry(Math.random())}
                setPresetRefresh={() => presets.setRetry(Math.random())}
              />
            ))}
            <AddPresetButton
              presetNames={Object.keys(presets.data)}
              setRetry={() => presets.setRetry(Math.random())}
            />
          </View>
        </CardContent>
      </Card>
    );
  };

  const ErrorCard = () => (
    <Card className="w-full shadow-lg border-destructive/20 bg-destructive/5">
      <CardContent className="pt-6">
        <View className="flex items-center gap-4">
          <View className="p-3 bg-destructive/10 rounded-full">
            <Activity className="text-destructive" width={24} height={24} />
          </View>
          <Text className="text-lg font-semibold text-destructive">
            Connection Error
          </Text>
          <Text className="text-center text-muted-foreground">
            {error?.message || "Unable to connect to LED matrix"}
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
              // Colors / TintColor can not be set as variable (this should be the same as the --primary css color)
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
                <StatusCard />
                <QuickActionsCard />
                <PresetsSection />
              </>
            )}
          </View>
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}