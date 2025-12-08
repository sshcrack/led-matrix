import { useLocalSearchParams } from 'expo-router';
import { ScrollView } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import ImageProviders from '~/components/modify-providers/ImageProviders';
import { Text } from '~/components/ui/text';

export default function ModifyPreset() {
    const local = useLocalSearchParams();
    const { preset_id, scene_id } = local

    if (typeof preset_id !== "string" || typeof scene_id !== "string")
        return <Text>Error: Invalid ID</Text>

    return <SafeAreaProvider>
        <SafeAreaView className="flex-1 w-full bg-background">
            <ScrollView className='flex-1 gap-5 p-4 bg-secondary/30 w-full' contentContainerStyle={{
                alignItems: "center"
            }}>
                <ImageProviders preset_id={preset_id} scene_id={scene_id} />
            </ScrollView>
        </SafeAreaView>
    </SafeAreaProvider >
}