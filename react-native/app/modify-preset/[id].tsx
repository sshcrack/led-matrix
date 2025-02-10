import { Text } from '@rn-primitives/label';
import { useLocalSearchParams } from 'expo-router';
import { RefreshControl, ScrollView } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { Preset } from '~/components/apiTypes/list_presets';
import useFetch from '~/components/useFetch';
import SceneComponent from './Scene';

export default function ModifyPreset() {
    const local = useLocalSearchParams();
    const id = local.id
    if (typeof id !== "string")
        return <Text>Error: Invalid ID</Text>

    const { data, error, isLoading, setRetry } = useFetch<Preset>(`/presets?id=${encodeURIComponent(id)}`)

    const Children = () => {
        if (isLoading)
            return <Text>Loading...</Text>
        if (error || !data)
            return <Text>Error: {error?.message ?? "Unknown Error"}</Text>

        return <>
            {data.scenes.map((data, i) => <SceneComponent key={i} data={data} />)}
        </>
    }


    return <SafeAreaProvider>
        <SafeAreaView className="flex-1" edges={['top']}>
            <ScrollView className='flex-1 gap-5 p-4 bg-secondary/30' contentContainerStyle={{
                alignItems: "center"
            }} refreshControl={
                <RefreshControl
                    refreshing={isLoading}
                    onRefresh={() => {
                        setRetry(Math.random())
                    }} />
            }>
                <Children />
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}