import { useLocalSearchParams } from 'expo-router';
import { RefreshControl, ScrollView, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { Preset } from '~/components/apiTypes/list_presets';
import useFetch from '~/components/useFetch';
import SceneComponent from '../../components/modify-preset/Scene';
import { ListScenes } from '~/components/apiTypes/list_scenes';
import { Text } from '~/components/ui/text';
import { ReactSetState } from '~/lib/utils';
import { useEffect, useState } from 'react';

type SceneWrapperProps = {
    isLoading: boolean,
    data: Preset | null,
    setData: ReactSetState<Preset | null>,
    listScenes: ListScenes[] | null,
    error: Error | null
    errorProperties: Error | null
}

function SceneWrapper({ data, setData, listScenes: listScenes, error, errorProperties, isLoading }: SceneWrapperProps) {
    if (isLoading)
        return <Text>Loading...</Text>

    if (error || !data || errorProperties || !listScenes)
        return <Text>Error: {error?.message ?? errorProperties?.message ?? "Unknown Error"}</Text>

    data.scenes.sort((a, b) => b.arguments.weight - a.arguments.weight)

    return <View className="w-full gap-5">
        {data.scenes.map(data => {
            const properties = listScenes.find(scene => scene.name === data.type)?.properties ?? []
            return <SceneComponent
                key={data.uuid}
                setSceneData={e => {
                    setData((prev) => {
                        if (!prev)
                            return prev

                        const scene = prev.scenes.find(e => e.uuid === data.uuid)
                        if (!scene)
                            return prev

                        const args = typeof e === "function" ? e(scene) : e

                        scene.arguments = args.arguments
                        return { ...prev }
                    })
                }}
                sceneData={data}
                properties={properties}
            />
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


    const [modifiedData, setModifiedData] = useState<Preset | null>(null)
    useEffect(() => {
        if (data)
            setModifiedData(data)
    }, [data])

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
                        setPropertyRetry(Math.random())
                    }} />
            }>
                <SceneWrapper
                    errorProperties={errorProperty}
                    listScenes={properties}
                    data={modifiedData}
                    setData={setModifiedData}
                    error={error}
                    isLoading={isLoading}
                />
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}