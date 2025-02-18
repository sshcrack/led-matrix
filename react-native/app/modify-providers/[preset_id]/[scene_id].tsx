import { useLocalSearchParams } from 'expo-router';
import { RefreshControl, ScrollView } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';
export default function ModifyPreset() {
    const local = useLocalSearchParams();

    const { preset_id, scene_id } = local
    if (typeof preset_id !== "string" || typeof scene_id !== "string")
        return <Text>Error: Invalid ID</Text>

    const { isLoading, setRetry, data, error } = useFetch(`/pixeljoint/providers?preset_id=${encodeURIComponent(preset_id)}&scene_id=${encodeURIComponent(scene_id)}`)
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
                <Text>Hi {JSON.stringify(data)}</Text>
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}