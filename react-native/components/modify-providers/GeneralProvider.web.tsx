import { WithSkiaWeb } from "@shopify/react-native-skia/lib/module/web";
import { View } from 'react-native';
import { titleCase } from '~/lib/utils';
import { ProviderValue } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import PagesProvider from './PagesProvider';
import { ProviderDataProvider } from './ProviderDataContext';

const providers = {
    "collection": () => (
        <WithSkiaWeb
            // import() uses the default export of MySkiaComponent.tsx
            opts={{locateFile: (_) => "/canvaskit.wasm"}}
            getComponent={() => import("./CollectionProvider")}
            fallback={<Text>Loading Skia...</Text>}
        />
    ),
    "pages": PagesProvider
}

export default function GeneralProvider({ data }: { data: ProviderValue[] }) {
    return <View className="w-full flex-1">
        {data.map((provider, index) => {
            const Provider = providers[provider.type]
            return <ProviderDataProvider data={provider}>
                <View key={index} className='w-full flex-1'>
                    <Text className="text-xl">{titleCase(provider.type)}</Text>
                    <Provider />
                </View>
            </ProviderDataProvider>
        })}
    </View>
}