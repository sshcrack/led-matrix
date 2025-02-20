import { useLocalSearchParams } from 'expo-router';
import { useEffect, useState } from 'react';
import { RefreshControl, ScrollView } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { ProviderValue } from '~/components/apiTypes/list_scenes';
import Loader from '~/components/Loader';
import GeneralProvider from '~/components/modify-providers/GeneralProvider';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';
export default function ModifyPreset() {
    const local = useLocalSearchParams();

    const { preset_id, scene_id } = local
    if (typeof preset_id !== "string" || typeof scene_id !== "string")
        return <Text>Error: Invalid ID</Text>

    const { isLoading, setRetry, data, error } = useFetch<ProviderValue[]>(`/pixeljoint/providers?preset_id=${encodeURIComponent(preset_id)}&scene_id=${encodeURIComponent(scene_id)}`)
    const [modifiedData, setModifiedData] = useState<ProviderValue[] | null>(data)

    useEffect(() => {
        setModifiedData(data)
    }, [data])

    return <SafeAreaProvider>
        <SafeAreaView className="flex-1 w-full" edges={['top']}>
            <ScrollView className='flex-1 gap-5 p-4 bg-secondary/30 w-full' contentContainerStyle={{
                alignItems: "center"
            }} refreshControl={
                <RefreshControl
                    refreshing={isLoading}
                    onRefresh={() => {
                        setRetry(Math.random())
                    }} />
            }>
                {modifiedData && <GeneralProvider data={modifiedData} setData={setModifiedData} />}
                {error && <Text>Error: {error.message ?? JSON.stringify(error)}</Text>}
                {isLoading && <>
                    <Loader />
                    <Text>Loading...</Text>
                </>}
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}