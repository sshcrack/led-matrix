import { useContext, useMemo, useState } from 'react';
import { View } from 'react-native';
import Toast from 'react-native-toast-message';
import { AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle } from '~/components/ui/alert-dialog';
import { Button } from '~/components/ui/button';
import { Input } from '~/components/ui/input';
import { Text } from '~/components/ui/text';
import { Plus } from '~/lib/icons/Plus';
import { CollectionShaderProvider as CollectionJson } from '../apiTypes/list_scenes';
import { ProviderDataContext } from './ProviderDataContext';
import { Trash2 } from '~/lib/icons/Trash2';

export interface ShaderUrlProp {
    id: number;
    url: string;
}

function isValidShadertoyUrl(s: string) {
    return s.startsWith("www.shadertoy.com") ||
        s.startsWith("http://www.shadertoy.com") ||
        s.startsWith("https://www.shadertoy.com");
}

export default function CollectionShaderProvider() {
    const [isDialogOpen, setIsDialogOpen] = useState(false)
    const [newUrl, setNewUrl] = useState("")

    const { data: untypedData, setData } = useContext(ProviderDataContext)
    const data = untypedData as CollectionJson

    const urls = useMemo(() => {
        if (!data)
            return null

        return data.arguments.urls.map((url, index) => ({
            id: index,
            url: url
        }))
    }, [data])

    if (!urls)
        return <Text>Loading...</Text>

    const addUrl = () => {
        if (!newUrl || newUrl.trim() === "") {
            Toast.show({
                type: 'error',
                text1: 'Error',
                text2: 'Please enter a URL',
            });
            return;
        }

        if (!isValidShadertoyUrl(newUrl)) {
            Toast.show({
                type: 'error',
                text1: 'Invalid URL',
                text2: 'URL must start with www.shadertoy.com, http://www.shadertoy.com, or https://www.shadertoy.com',
            });
            return;
        }

        setData(e => {
            if (!e)
                return e

            const copy = JSON.parse(JSON.stringify(e)) as CollectionJson
            copy.arguments.urls.push(newUrl)
            return copy
        })

        setNewUrl("")
        setIsDialogOpen(false)
    }

    const removeUrl = (id: number) => {
        setData(e => {
            if (!e)
                return e

            const copy = JSON.parse(JSON.stringify(e)) as CollectionJson
            copy.arguments.urls.splice(id, 1)
            return copy
        })
    }

    return (
        <View className='gap-5 pb-5'>
            <View className='flex-row items-center justify-between'>
                <Text className='font-semibold'>Shader URLs ({urls.length})</Text>
                <Button
                    size="icon"
                    variant="outline"
                    onPress={() => setIsDialogOpen(true)}
                >
                    <Plus />
                </Button>
            </View>

            <View className='gap-2'>
                {urls.length === 0 && (
                    <Text className='text-muted-foreground'>No shader URLs added yet</Text>
                )}
                {urls.map((item, index) => (
                    <View key={item.id} className='flex-row items-center gap-2'>
                        <Button
                            size="icon"
                            variant="ghost"
                            onPress={() => removeUrl(index)}
                        >
                            <Trash2 className='text-red-500' />
                        </Button>
                        <Text className='flex-1 text-sm' numberOfLines={1}>{item.url}</Text>
                    </View>
                ))}
            </View>

            <AlertDialog open={isDialogOpen} onOpenChange={setIsDialogOpen}>
                <AlertDialogContent>
                    <AlertDialogHeader>
                        <AlertDialogTitle>Add Shader URL</AlertDialogTitle>
                    </AlertDialogHeader>
                    <View className='gap-4'>
                        <Text className='text-sm text-muted-foreground'>
                            Enter a Shadertoy URL (must start with www.shadertoy.com)
                        </Text>
                        <Input
                            placeholder="https://www.shadertoy.com/view/..."
                            value={newUrl}
                            onChangeText={setNewUrl}
                            autoCapitalize="none"
                        />
                    </View>
                    <AlertDialogFooter>
                        <AlertDialogCancel>
                            <Text>Cancel</Text>
                        </AlertDialogCancel>
                        <AlertDialogAction onPress={addUrl}>
                            <Text>Add</Text>
                        </AlertDialogAction>
                    </AlertDialogFooter>
                </AlertDialogContent>
            </AlertDialog>
        </View>
    );
}
