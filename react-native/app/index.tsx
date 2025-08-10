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
import useFetch from '~/components/useFetch';
import StatusCard from '~/components/home/StatusCard';
import QuickActionsCard from '~/components/home/QuickActionsCard';
import PresetsSection from '~/components/home/PresetsSection';
import ErrorCard from '~/components/home/ErrorCard';

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
              <ErrorCard error={error} setRetry={setRetry} />
            ) : (
              <>
                <StatusCard
                  turnedOn={turnedOn}
                  settingStatus={settingStatus}
                  setSettingStatus={setSettingStatus}
                  setTurnedOn={setTurnedOn}
                />
                <QuickActionsCard
                  isWeb={isWeb}
                  updateStatus={updateStatus}
                  setRetry={setRetry}
                />
                <PresetsSection
                  status={status}
                  presets={presets}
                  isWeb={isWeb}
                />
              </>
            )}
          </View>
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}