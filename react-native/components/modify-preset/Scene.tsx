import { Text } from '~/components/ui/text';
import { View } from 'react-native';
import { Preset, Scene } from '~/components/apiTypes/list_presets';
import { Property } from '../apiTypes/list_scenes';
import { DynamicPluginProperty } from './property_list';
import { ReactSetState, titleCase } from '~/lib/utils';

export type SceneComponentProps = {
    sceneData: Scene,
    properties: Property<any>[],
    setSceneData: ReactSetState<Scene>
}

export default function SceneComponent({ sceneData: data, setSceneData, properties }: SceneComponentProps) {
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
                    setScene={e => {
                        setSceneData(e)
                    }}
                />
            })
        }
    </View>
}