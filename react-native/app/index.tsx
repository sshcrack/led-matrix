import * as React from 'react';
import { RefreshControl, ScrollView, StatusBar, StyleSheet, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { ListPresets } from '~/components/apiTypes/list_presets';
import { Status } from '~/components/apiTypes/status';
import Preset from '~/components/home/Preset';
import { Button } from '~/components/ui/button';
import { Label } from '~/components/ui/label';
import { Switch } from '~/components/ui/switch';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';


export default function Screen() {
  const presets = useFetch<ListPresets>(`/list_presets`);
  const status = useFetch<Status>(`/status`);

  const [settingStatus, setSettingStatus] = React.useState(false);
  const [turnedOn, setTurnedOn] = React.useState<null | boolean>(null);

  const setRetry = () => {
    setTurnedOn(null)
    setSettingStatus(false)
    presets.setRetry(Math.random())
    status.setRetry(Math.random())
  }

  React.useEffect(() => {
    if (!settingStatus)
      return

    fetch(`${process.env.EXPO_PUBLIC_API_URL}/set_enabled?enabled=${turnedOn ? "true" : "false"}`)
      .catch(e => {
        Toast.show({
          type: "error",
          text1: "Error setting status",
          text2: e.message
        })
      })
      .finally(() => setSettingStatus(false))
  }, [turnedOn, settingStatus])

  React.useEffect(() => {
    if (status.data && turnedOn === null) {
      setTurnedOn(!status.data.turned_off);
    }
  }, [status.data])

  const isLoading = presets.isLoading || status.isLoading;
  const error = presets.error ?? status.error;
  const hasData = presets.data && status.data;

  const OnLoadChildren = () => <>
    <Text className='text-2xl'>LED Matrix Status</Text>
    <View className='flex-row items-center gap-2'>
      <Switch disabled={turnedOn === null || settingStatus} checked={turnedOn ?? false} onCheckedChange={v => {
        setSettingStatus(true)
        setTurnedOn(v);
      }} nativeID='led-matrix-status' />
      <Label
        nativeID='led-matrix-status'
        disabled={turnedOn === null || settingStatus}
        onPress={() => {
          setSettingStatus(true)
          setTurnedOn((prev) => !prev); 5
        }}
      >
        {settingStatus ? "Setting..." : turnedOn ? "Enabled" : "Disabled"}
      </Label>
    </View>
    <Text className="text-2xl pt-5 pb-5">Presets</Text>
    <View className='flex flex-row flex-wrap gap-5 w-full items-center'>
      {presets.data && Object.entries(presets.data).map(([key, preset]) => {
        return <Preset key={Math.random()} preset={preset} name={key} isActive={status.data?.current === key} />
      })}
    </View>
  </>

  return (
    <SafeAreaProvider>
      <SafeAreaView className="flex-1" edges={['top']}>
        <ScrollView className='flex-1 gap-5 p-4 bg-secondary/30' contentContainerStyle={{
          alignItems: "center"
        }} refreshControl={
          <RefreshControl
            refreshing={isLoading}
            onRefresh={() => {
              setRetry()
            }} />
        }>
          {error && <View className="flex gap-5 items-center w-full">
            <Text>Error: {error.message}</Text>
            <Button onPress={() => setRetry()}><Text>Retry</Text></Button>
          </View>}
          {hasData && <OnLoadChildren />}
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider >
  );
}