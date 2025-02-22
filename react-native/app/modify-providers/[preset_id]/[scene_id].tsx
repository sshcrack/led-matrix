import { useLocalSearchParams } from 'expo-router';
import { getPathWithConventionsCollapsed } from 'expo-router/build/fork/getPathFromState-forks';
import { useEffect, useRef, useState } from 'react';
import { ScrollView } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { ProviderValue } from '~/components/apiTypes/list_scenes';
import { useSubConfig } from '~/components/configShare/ConfigProvider';
import Loader from '~/components/Loader';
import GeneralProvider from '~/components/modify-providers/GeneralProvider';
import { Text } from '~/components/ui/text';

export default function ModifyPreset() {
    const local = useLocalSearchParams();
    const modDataRef = useRef<ProviderValue[] | null>(null)
    const { preset_id, scene_id } = local

    if (typeof preset_id !== "string" || typeof scene_id !== "string")
        return <Text>Error: Invalid ID</Text>

    const { config, setSubConfig } = useSubConfig<ProviderValue[] | null>(preset_id, ["scenes", scene_id, "arguments", "providers"])
    const [modifiedData, setModifiedData] = useState<ProviderValue[] | null>(config)

    useEffect(() => {
        console.log("Setting modified data")
        setModifiedData(config)
    }, [config])

    useEffect(() => {
        modDataRef.current = modifiedData
    }, [modifiedData])

    useEffect(() => {
        console.log("Setting up save config listener")
        return () => {
            if (modDataRef.current)
                setSubConfig(modDataRef.current)
        }
    }, [])

    return <SafeAreaProvider>
        <SafeAreaView className="flex-1 w-full">
            <ScrollView className='flex-1 gap-5 p-4 bg-secondary/30 w-full' contentContainerStyle={{
                alignItems: "center"
            }}>
                {!modifiedData && <Loader />}
                {modifiedData && <GeneralProvider data={modifiedData} setData={setModifiedData} />}
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}