import { useLocalSearchParams } from 'expo-router';
import { useContext, useEffect, useMemo } from 'react';
import { RefreshControl, ScrollView } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { arrayToObjectPresets, Preset, RawPreset } from '~/components/apiTypes/list_presets';
import { ListScenes } from '~/components/apiTypes/list_scenes';
import { ConfigContext } from '~/components/configShare/ConfigProvider';
import AddScene from '~/components/modify-preset/AddScene';
import ExitConfirmation from '~/components/modify-preset/ExitConfirmation';
import { PresetIdContext } from '~/components/modify-preset/PresetIdProvider';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';
import SceneComponent from '../../components/modify-preset/Scene';

type SceneWrapperProps = {
    isLoading: boolean,
    data: Preset | undefined,
    listScenes: ListScenes[] | null,
    error: Error | null
    errorProperties: Error | null
}

function SceneWrapper({ data, listScenes: listScenes, error, errorProperties, isLoading }: SceneWrapperProps) {
    if (isLoading)
        return <Text>Loading...</Text>

    if (error || !data || errorProperties || !listScenes)
        return <Text>Error: {error?.message ?? errorProperties?.message ?? "Unknown Error"}</Text>

    const entries = useMemo(() => {
        const e = Object.entries(data.scenes)
        e.sort(([_a, a], [_b, b]) => b.arguments.weight - a.arguments.weight)

        return e
    }, [data.scenes])


    return <>
        {entries.map(([key, value]) => {
            const properties = listScenes.find(scene => scene.name === value.type)?.properties ?? []
            return <SceneComponent
                key={key}
                sceneId={key}
                properties={properties}
            />
        })}
        <AddScene scenes={listScenes} />
    </>
}

export default function ModifyPreset() {
    const local = useLocalSearchParams();
    const preset_id = local.preset_id
    if (typeof preset_id !== "string")
        return <Text>Error: Invalid ID</Text>



    const { data, error, isLoading: isLoadingPreset, setRetry } = useFetch<RawPreset>(`/presets?id=${encodeURIComponent(preset_id)}`)
    const { data: properties, error: errorProperty, isLoading: isLoadingProperty, setRetry: setPropertyRetry } = useFetch<ListScenes[]>(`/list_scenes`)
    const { config, update, setConfig } = useContext(ConfigContext)


    useEffect(() => {
        if (data) {
            setConfig(e => {
                const curr = new Map(e)
                curr.set(preset_id, arrayToObjectPresets(data))

                return curr
            })
        }
    }, [data])

    useEffect(() => {
        setRetry(Math.random())
    }, [update])



    const isLoading = isLoadingPreset || isLoadingProperty
    const preset = useMemo(() => config.get(preset_id), [config, preset_id])
    return <SafeAreaProvider>
        <SafeAreaView className="flex-1 bg-background">
            <ScrollView className='flex-1 gap-5 m-5' contentContainerStyle={{
                alignItems: "center",
                paddingBottom: 100  // Added more bottom padding
            }} refreshControl={
                <RefreshControl
                    refreshing={isLoading}
                    onRefresh={() => {
                        setRetry(Math.random())
                        setPropertyRetry(Math.random())
                    }} />
            }>
                <PresetIdContext.Provider value={preset_id}>
                    <SceneWrapper
                        errorProperties={errorProperty}
                        data={preset}
                        listScenes={properties}
                        error={error}
                        isLoading={isLoading}
                    />
                    <ExitConfirmation data={data} />
                </PresetIdContext.Provider>
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider>
}