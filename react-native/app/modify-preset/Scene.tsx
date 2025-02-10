import { Text, View } from '@rn-primitives/slot';
import { Scene } from '~/components/apiTypes/list_presets';

export default function SceneComponent({ data }: { data: Scene }) {
    return <View>
        <Text>{data.type}</Text>
    </View>
}