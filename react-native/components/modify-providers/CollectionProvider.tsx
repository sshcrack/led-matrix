import { View } from 'react-native';
import { ResponsiveGrid } from 'react-native-flexible-grid';
import { CollectionProvider as CollectionJson } from '../apiTypes/list_scenes';
import { getImageUrl } from '~/lib/utils';
import { Canvas, Circle, Group, Image, useImage } from '@shopify/react-native-skia';
import Loader from '../Loader';
import { useProviderData } from './ProviderDataContext';
import { useAnimatedStyle, useSharedValue } from 'react-native-reanimated';
import { useEffect, useLayoutEffect, useState } from 'react';
import { Text } from '../ui/text';

interface DataProp {
    id: number;
    imageUrl: string;
}

function CollectionItem({ item }: { item: DataProp }) {
    const image = useImage(item.imageUrl, e => console.error(e))
    const size = useSharedValue({ width: 0, height: 0 });


    const props = useAnimatedStyle(() => {
        console.log(size.value)
        return {
            width: size.value.width,
            height: 100
        }
    })

    return (
        <View className='background-black p-1 w-full h-[100px]'>
            {image && <Canvas onSize={size} className='h-[100px] w-full justify-center items-center rounded-xl'>
                <Image x={0} y={0} fit="contain" image={image} {...props} />
            </Canvas>}
            {!image && <Text>Loading...</Text>}
        </View>
    );
}


export default function CollectionProvider() {
    const data = useProviderData<CollectionJson>()

    const renderItem = ({ item }: { item: DataProp }) => <CollectionItem item={item} />;

    return <View className='w-full flex-1'>
        <ResponsiveGrid
            maxItemsPerColumn={2}
            data={data.arguments.map((imageUrl, index) => ({ id: index, imageUrl: getImageUrl(imageUrl) }))}
            renderItem={renderItem}
            showScrollIndicator
            keyExtractor={(item: DataProp) => item.id.toString()}
        />
    </View>
}