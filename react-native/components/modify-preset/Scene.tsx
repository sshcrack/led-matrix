import { format } from '@lukeed/ms';
import { useMemo, useState } from 'react';
import { View } from 'react-native';
import Animated, { Easing, useAnimatedStyle, useSharedValue, withTiming } from 'react-native-reanimated';
import { ListPresets, RawPreset, Scene } from '~/components/apiTypes/list_presets';
import { Text } from '~/components/ui/text';
import { ChevronDown } from '~/lib/icons/ChevronDown';
import { ReactSetState, titleCase } from '~/lib/utils';
import { Property } from '../apiTypes/list_scenes';
import { Collapsible, CollapsibleContent, CollapsibleTrigger } from '../ui/collapsible';
import { DynamicPluginProperty } from './property_list';
import { useSubConfig } from '../configShare/ConfigProvider';
import usePresetId from './PresetIdProvider';

export type SceneComponentProps = {
    sceneId: string,
    properties: Property<any>[]
}

export default function SceneComponent({ sceneId, properties }: SceneComponentProps) {
    const [isOpen, setOpen] = useState(true)
    const rotation = useSharedValue(0);
    const presetId = usePresetId()

    const { config } = useSubConfig<Scene>(presetId, ["scenes", sceneId])
    const animatedStyle = useAnimatedStyle(() => ({
        transform: [{ rotate: `${rotation.value}deg` }],
    }));


    const entries = useMemo(() => {
        const arr = Object.entries(config.arguments)
        arr.sort(([a], [b]) => {
            if (a === "weight" || a === "duration")
                return -1

            if (b === "weight" || b === "duration")
                return 1

            return 0
        })

        return arr
    }, [config.arguments])

    const weight = useMemo(() => {
        return config.arguments["weight"] ?? 0
    }, [config.arguments])

    const duration = useMemo(() => {
        return config.arguments["duration"] ?? 0
    }, [config.arguments])

    return <Collapsible open={isOpen} onOpenChange={e => {
        setOpen(e)

        rotation.value = withTiming(e ? 180 : 0, { easing: Easing.inOut(Easing.quad) });
    }} className='w-full gap-6'>
        <CollapsibleTrigger className='flex-row gap-2 items-center w-full'>
            <Text className='text-center text-2xl font-semibold'>{titleCase(config.type)}</Text>
            <Text>{format(duration)}</Text>
            <View className='flex-row flex-1 justify-end pr-3 gap-5'>
                <Text>{weight}</Text>
            </View>
            <Animated.View
                className="justify-self-end"
                style={[animatedStyle]}
            >
                <ChevronDown className="text-foreground" />
            </Animated.View>
        </CollapsibleTrigger>
        <CollapsibleContent className="align-center w-full pb-10 flex-col gap-5">
            {
                entries.map(([propertyName, value]) => {
                    const property = properties.find(property => property.name === propertyName)
                    if (!property)
                        return <Text>Unknown Property {propertyName}</Text>

                    return <DynamicPluginProperty
                        key={propertyName}
                        propertyName={propertyName}
                        typeId={property.type_id}
                        defaultVal={property.default_value}
                        value={value}
                        sceneId={config.uuid}
                    />
                })
            }
        </CollapsibleContent>
    </Collapsible>
}