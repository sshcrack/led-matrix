import { Text } from '@rn-primitives/label';
import { View } from 'react-native';
import { Scene } from '~/components/apiTypes/list_presets';
import { Property } from '../apiTypes/list_scenes';
import PropertyManage from './Property';
import DurationProperty from './DurationProperty';
import WeightProperty from './WeightProperty';

export default function SceneComponent({ data, properties }: { data: Scene, properties: Property[] }) {
    const entries = Object.entries(data.arguments)

    return <View className="align-center w-full pb-10">
        <Text className='text-center text-2xl font-semibold'>{data.type}</Text>
        {
            entries.map(([propertyName, value]) => {
                if(propertyName === "duration") {
                    return <DurationProperty key={propertyName} value={value}/>
                }

                if(propertyName === "weight") {
                    return <WeightProperty key={propertyName} value={value} />
                }

                const property = properties.find(property => property.name === propertyName)
                if (!property) return <Text>Property {propertyName} could not be found.</Text>

                const defaultVal = property?.default_value

                return <PropertyManage
                    key={propertyName}
                    propertyName={propertyName}
                    defaultVal={defaultVal}
                    property={property}
                    value={value}
                />
            })
        }
    </View>
}