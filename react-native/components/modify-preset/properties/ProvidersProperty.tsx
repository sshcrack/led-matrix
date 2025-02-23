import { useRouter } from 'expo-router';
import { View } from 'react-native';
import { ProviderValue } from '~/components/apiTypes/list_scenes';
import { Button } from '~/components/ui/button';
import { Text } from '~/components/ui/text';
import { titleCase } from '~/lib/utils';
import usePresetId from '../PresetIdProvider';
import { PluginPropertyProps } from '../property_list';
import { useSceneContext } from '../SceneContext';

export default function ProvidersProperty({ propertyName }: Omit<PluginPropertyProps<ProviderValue>, 'setValue'>) {
    const { sceneId } = useSceneContext()
    const presetId = usePresetId()
    const router = useRouter()

    return <View className='flex-row gap-2 w-full justify-between items-center'>
        <Text className='font-semibold self-center'>{titleCase(propertyName)}</Text>
        <View className='w-1/2 gap-2 flex-row items-center'>
            <Button
                className='flex-1'
                onPress={() => {
                    console.log("Opening modify providers")
                    router.push({
                        pathname: '/modify-providers/[preset_id]/[scene_id]',
                        params: { preset_id: presetId, scene_id: sceneId },
                    })
                }}
            >
                <Text>Configure</Text>
            </Button>
        </View>
    </View>
}