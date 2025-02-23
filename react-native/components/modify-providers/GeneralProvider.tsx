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

const providers = {
    "collection": CollectionProvider,
    "pages": PagesProvider
}

export default function GeneralProvider({ data, preset_id, scene_id, setData }: { data: ProviderValue[], preset_id: string, scene_id: string, setData: ReactSetState<ProviderValue[] | null> }) {
    const { data: providerData } = useFetch<ListProviders[]>(`/list_providers`)
    if (!providerData)
        return <Loader />

    console.log("Data", data)
    return <View className="w-full flex-1">
        {data.map((provider, index) => {
            const Provider = providers[provider.type as keyof typeof providers]
            if (!Provider)
                return <Text key={index}>Unknown provider type: {provider.type}</Text>

            return <ProviderDataProvider key={index} data={provider} setData={x => {
                if (typeof x === "function") {
                    setData(e => {
                        if (!e)
                            return e

                        const copy = JSON.parse(JSON.stringify(e))
                        copy[index] = x(copy[index])

                        return copy
                    })
                } else {
                    if (!data)
                        return

                    const copy = JSON.parse(JSON.stringify(data))
                    copy[index] = x

                    setData(copy)
                }
            }}>
                <View className='w-full flex-1'>
                    <View className="flex-row">
                        <Text className="text-xl">{titleCase(provider.type)}</Text>
                        <Button size="icon" variant="ghost" className='p-5' onPress={() => {
                            setData(e => {
                                if (!e)
                                    return e

                                const copy = JSON.parse(JSON.stringify(e))
                                copy.splice(index, 1)

                                return copy
                            })
                        }}>
                            <Trash2 className='text-red-500' />
                        </Button>
                    </View>
                    <Provider />
                </View>
            </ProviderDataProvider>
        })}
        <AddProviderButton providers={providerData} sceneId={scene_id} presetId={preset_id} />
    </View>
}