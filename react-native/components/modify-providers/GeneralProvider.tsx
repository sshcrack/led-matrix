import { View } from 'react-native';
import { titleCase } from '~/lib/utils';
import { ProviderValue } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import CollectionProvider from './CollectionProvider';
import PagesProvider from './PagesProvider';
import { ProviderDataProvider } from './ProviderDataContext';

const providers = {
    "collection": CollectionProvider,
    "pages": PagesProvider
}

export default function GeneralProvider({ data }: { data: ProviderValue[] }) {
    return <View className="w-full flex-1">
        {data.map((provider, index) => {
            const Provider = providers[provider.type]
            return <ProviderDataProvider key={index} data={provider}>
                <View className='w-full flex-1'>
                    <Text className="text-xl">{titleCase(provider.type)}</Text>
                    <Provider />
                </View>
            </ProviderDataProvider>
        })}
    </View>
}