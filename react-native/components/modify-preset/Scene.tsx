import { Text } from '~/components/ui/text';
import { View } from 'react-native';
import { Scene } from '~/components/apiTypes/list_presets';
import { Property } from '../apiTypes/list_scenes';
import { DynamicPluginProperty } from './property_list';
import { titleCase } from '~/lib/utils';

export default function SceneComponent({ data, properties }: { data: Scene, properties: Property<any>[] }) {
    const entries = Object.entries(data.arguments)

    return <View className="align-center w-full pb-10 flex-col gap-5">
        <Text className='text-center text-2xl font-semibold'>{titleCase(data.type)}</Text>
        {
            entries.map(([propertyName, value]) => {
                const property = properties.find(property => property.name === propertyName)
                const defaultVal = property?.default_value

                return <DynamicPluginProperty
                    key={propertyName}
                    propertyName={propertyName}
                    defaultVal={defaultVal}
                    value={value}
                />
            })
        }
    </View>
}