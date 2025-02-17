import { useLocalSearchParams } from 'expo-router';
import { RefreshControl, ScrollView, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { Preset } from '~/components/apiTypes/list_presets';
import useFetch from '~/components/useFetch';
import SceneComponent from '../../components/modify-preset/Scene';
import { ListScenes, Property } from '~/components/apiTypes/list_scenes';
import { Text } from '~/components/ui/text';

type SceneWrapperProps = {
    isLoading: boolean,
    data: Preset | null,
    listScenes: ListScenes[] | null,
    error: Error | null
    errorProperties: Error | null
}

function SceneWrapper({ data, listScenes: listScenes, error, errorProperties, isLoading }: SceneWrapperProps) {
    if (isLoading)
        return <Text>Loading...</Text>

    if (error || !data || errorProperties || !listScenes)
        return <Text>Error: {error?.message ?? errorProperties?.message ?? "Unknown Error"}</Text>

    data.scenes.sort((a, b) => b.arguments.weight - a.arguments.weight)

    return <View className="w-full gap-5">
        {data.scenes.map((data, i) => {
            const properties = listScenes.find(scene => scene.name === data.type)?.properties ?? []
            return <SceneComponent key={i} data={data} properties={properties}/>
        })}
    </View>
}

export default function ModifyPreset() {
    const local = useLocalSearchParams();
    const id = local.id
    if (typeof id !== "string")
        return <Text>Error: Invalid ID</Text>

    const { data, error, isLoading: isLoadingPreset, setRetry } = useFetch<Preset>(`/presets?id=${encodeURIComponent(id)}`)
    const { data: properties, error: errorProperty, isLoading: isLoadingProperty, setRetry: setPropertyRetry } = useFetch<ListScenes[]>(`/list_scenes`)


    const isLoading = isLoadingPreset || isLoadingProperty
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
                <SceneWrapper errorProperties={errorProperty} listScenes={properties} data={data} error={error} isLoading={isLoading} />
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}