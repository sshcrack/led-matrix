import { View } from 'react-native';
import { titleCase } from '~/lib/utils';
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

const imageProviders = {
    "collection": CollectionProvider,
    "pages": PagesProvider,
}

export default function ImageProviders({ preset_id, scene_id }: { preset_id: string, scene_id: string }) {
    const { config, setSubConfig } = useSubConfig<ProviderValue[] | null>(preset_id, ["scenes", scene_id, "arguments", "providers"])
    const { data: providerData } = useFetch<ListProviders[]>(`/list_providers`)

    if (!providerData)
        return <Loader />

    return <View className="w-full flex-1">
        {config?.map((provider, index) => {
            const Provider = imageProviders[provider.type as keyof typeof imageProviders]
            if (!Provider)
                return <Text key={index}>Unknown provider type: {provider.type}</Text>

            return <ProviderDataProvider key={index} data={provider} setData={x => {
                if (typeof x === "function") {
                    setSubConfig(e => {
                        if (!e)
                            return e

                        const copy = JSON.parse(JSON.stringify(e))
                        const res = x(copy[index]);
                        if (!res)
                            return e

                        copy[index] = res

                        console.log("Setting to", copy)
                        return copy
                    })
                } else {
                    if (!config || !x)
                        return

                    const copy = JSON.parse(JSON.stringify(config))
                    copy[index] = x

                    console.log("Setting to", copy)
                    setSubConfig(copy)
                }
            }}>
                <View className='w-full flex-1'>
                    <View className="flex-row mb-5 items-center">
                        <Button size="icon" variant="ghost" className='mr-5' onPress={() => {
                            setSubConfig(e => {
                                if (!e)
                                    return e

                                const copy = JSON.parse(JSON.stringify(e))
                                copy.splice(index, 1)

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
        <AddProviderButton providers={providerData} sceneId={scene_id} presetId={preset_id}  providerKey='providers'/>
    </View>
}
