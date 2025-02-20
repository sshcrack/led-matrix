import { View } from 'react-native';
import { ReactSetState, titleCase } from '~/lib/utils';
import { ProviderValue } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import CollectionProvider from './collection/CollectionProvider';
import PagesProvider from './PagesProvider';
import { ProviderDataProvider } from './ProviderDataContext';

const providers = {
    "collection": CollectionProvider,
    "pages": PagesProvider
}

export default function GeneralProvider({ data, setData }: { data: ProviderValue[], setData: ReactSetState<ProviderValue[] | null> }) {
    return <View className="w-full flex-1">
        {data.map((provider, index) => {
            const Provider = providers[provider.type]
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
                    <Text className="text-xl">{titleCase(provider.type)}</Text>
                    <Provider />
                </View>
            </ProviderDataProvider>
        })}
    </View>
}