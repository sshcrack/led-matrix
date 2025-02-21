import { Canvas, Image, useImage } from '@shopify/react-native-skia';
import { useContext } from 'react';
import { Image as NativeImage, Platform, View } from 'react-native';
import Toast from 'react-native-toast-message';
import { Button } from '~/components/ui/button';
import { ContextMenu, ContextMenuContent, ContextMenuItem, ContextMenuTrigger } from '~/components/ui/context-menu';
import { Text } from '~/components/ui/text';
import { ProviderDataContext } from '../ProviderDataContext';
import type { DataProp } from './CollectionProvider';
import { CollectionProvider } from '~/components/apiTypes/list_scenes';


export default function CollectionItem({ item, index }: { item: DataProp, index: number }) {
    const isWeb = Platform.OS === "web"

    const { setData } = useContext(ProviderDataContext)
    const image = !isWeb && useImage(item.imageUrl, e => {
        Toast.show({
            type: "error",
            text1: "Error loading image",
            text2: e.message ?? JSON.stringify(e)
        })
        console.error("Error loading image", e)
    })

    const NonWebComps = () => {
        return image ? <Canvas style={{ height: 128, width: 128 }}>
            <Image x={0} y={0} image={image} width={128} height={128} />
        </Canvas> : <Text className='text-background'>Loading...</Text>
    }

    return (
        <ContextMenu>
            <ContextMenuTrigger>
                <View className='bg-black text-slate-800 shadow p-3 justify-center items-center h-[128px] w-[128px]'>
                    {isWeb ?
                        <img src={item.imageUrl} style={{
                            objectFit: "contain",
                            imageRendering: "pixelated"
                        }} />
                        :
                        <NonWebComps />
                    }
                </View>
            </ContextMenuTrigger>
            <ContextMenuContent>
                <ContextMenuItem closeOnPress inset asChild>
                    <Button variant="ghost" onPress={() => {
                        setData(e => {
                            const data = e as CollectionProvider
                            const copy: CollectionProvider = JSON.parse(JSON.stringify(data))
                            copy.arguments.splice(index, 1)

                            return copy
                        })
                    }}>
                        <Text className="text-red-500">Remove</Text>
                    </Button>
                </ContextMenuItem>
            </ContextMenuContent>
        </ContextMenu>

    );
}