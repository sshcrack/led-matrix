import { Text } from '@rn-primitives/label';
import { useLocalSearchParams } from 'expo-router';

export default function ModifyPreset() {
    const local = useLocalSearchParams();

    const id = local.id
    return <>
        <Text>{id}</Text>
    </>
}