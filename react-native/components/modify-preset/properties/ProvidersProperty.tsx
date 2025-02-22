import { Link } from 'expo-router';
import { View } from 'react-native';
import { ProviderValue } from '~/components/apiTypes/list_scenes';
import { Button } from '~/components/ui/button';
import { Text } from '~/components/ui/text';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { titleCase } from '~/lib/utils';
import { PluginPropertyProps } from '../property_list';
import { usePropertyUpdate, useSceneContext } from '../SceneContext';
import usePresetId from '../PresetIdProvider';

export default function ProvidersProperty({ defaultVal, propertyName }: Omit<PluginPropertyProps<ProviderValue>, 'setValue'>) {
    const { sceneId } = useSceneContext()
    const presetId = usePresetId()

    const setValue = usePropertyUpdate(propertyName);
    return <View className='flex-row gap-2 w-full justify-between items-center'>
        <Text className='font-semibold self-center'>{titleCase(propertyName)}</Text>
        <View className='w-1/2 gap-2 flex-row items-center'>
            <Button
                variant="secondary"
                size="icon"
                onPress={() => setValue(defaultVal)}
            >
                <RotateCcw className='text-foreground' />
            </Button>
            <Link push asChild href={{
                pathname: '/modify-providers/[preset_id]/[scene_id]',
                params: { preset_id: presetId, scene_id: sceneId },
            }}>
                <Button className='flex-1'><Text>Configure</Text></Button>
            </Link>
        </View>
    </View>
}