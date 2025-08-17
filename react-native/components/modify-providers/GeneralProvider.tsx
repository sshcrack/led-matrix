import { View } from 'react-native';
import { ReactSetState, titleCase } from '~/lib/utils';
import { ListProviders, ProviderValue } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import AddProviderButton from './AddProviderButton';
import CollectionProvider from './collection/CollectionProvider';
import PagesProvider from './PagesProvider';
import { ProviderDataProvider } from './ProviderDataContext';
import useFetch from '../useFetch';
import Loader from '../Loader';
import { Button } from '../ui/button';
import { Trash2 } from '~/lib/icons/Trash2';
import { useSubConfig } from '../configShare/ConfigProvider';

const providers = {
    "collection": CollectionProvider,
    "pages": PagesProvider
}

export default function GeneralProvider({ preset_id, scene_id }: { preset_id: string, scene_id: string }) {
    const { data: providerData } = useFetch<ListProviders[]>(`/list_providers`)
    const { config, setSubConfig } = useSubConfig<ProviderValue[] | null>(preset_id, ["scenes", scene_id, "arguments", "providers"])

    if (!providerData)
        return <Loader />


    return <View className="w-full flex-1">
        {config?.map((provider) => {
            const Provider = providers[provider.type as keyof typeof providers]
            const key = (provider as any).uuid ?? provider.type
            if (!Provider)
                return <Text key={key}>Unknown provider type: {provider.type}</Text>

            return <ProviderDataProvider key={key} data={provider} setData={x => {
                // Find the provider index by uuid so updates remain stable even if order changes
                setSubConfig(prev => {
                    if (!prev)
                        return prev

                    const copy = JSON.parse(JSON.stringify(prev)) as any[]
                    const idx = copy.findIndex((p: any) => (p && p.uuid) ? p.uuid === (provider as any).uuid : false)
                    if (idx === -1)
                        return prev

                    if (typeof x === "function") {
                        const res = x(copy[idx]);
                        if (!res)
                            return prev

                        copy[idx] = res
                        return copy
                    } else {
                        if (!x)
                            return prev

                        copy[idx] = x
                        return copy
                    }
                })
            }}>
                <View className='w-full flex-1'>
                    <View className="flex-row mb-5 items-center">
                        <Button size="icon" variant="ghost" className='mr-5' onPress={() => {
                            setSubConfig(prev => {
                                if (!prev)
                                    return prev

                                const copy = JSON.parse(JSON.stringify(prev)) as any[]
                                const idx = copy.findIndex((p: any) => (p && p.uuid) ? p.uuid === (provider as any).uuid : false)
                                if (idx === -1)
                                    return prev

                                copy.splice(idx, 1)

                                return copy
                            })
                        }}>
                            <Trash2 className='text-red-500' />
                        </Button>
                        <Text className="text-xl">{titleCase(provider.type)}</Text>
                    </View>
                    <Provider />
                </View>
            </ProviderDataProvider>
        })}
        <AddProviderButton providers={providerData} sceneId={scene_id} presetId={preset_id} />
    </View>
}