import * as ImagePicker from 'expo-image-picker';
import { useContext, useMemo, useState } from 'react';
import { View } from 'react-native';
import Toast from 'react-native-toast-message';
import { UploadImgResponse } from '~/components/apiTypes/pixeljoint/upload_img';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import Loader from '~/components/Loader';
import { AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle } from '~/components/ui/alert-dialog';
import { Button } from '~/components/ui/button';
import { DropdownMenu, DropdownMenuContent, DropdownMenuItem, DropdownMenuTrigger } from '~/components/ui/dropdown-menu';
import { Input } from '~/components/ui/input';
import { Text } from '~/components/ui/text';
import { FilePlus2 } from '~/lib/icons/FilePlus2';
import { Paperclip } from '~/lib/icons/Paperclip';
import { Plus } from '~/lib/icons/Plus';
import { getImageUrl } from '~/lib/utils';
import { CollectionProvider as CollectionJson } from '../../apiTypes/list_scenes';
import { ProviderDataContext } from '../ProviderDataContext';
import CollectionItem from './CollectionItem';


export interface DataProp {
    id: number;
    imageUrl: string;
}

function isValidHttpUrl(s: string) {
    let url;

    try {
        url = new URL(s);
    } catch (_) {
        return false;
    }

    return url.protocol === "http:" || url.protocol === "https:";
}

function formDataFromImagePicker(asset: ImagePicker.ImagePickerAsset) {
    const formData = new FormData();

    // @ts-expect-error: special react native format for form data
    formData.append("photo", asset.file ?? {
        uri: asset.uri,
        name: asset.fileName ?? asset.uri.split("/").pop(),
        type: asset.mimeType,
    });
    return formData;
}

export default function CollectionProvider() {
    const [uploading, setUploading] = useState(false)
    const [isDialogOpen, setIsDialogOpen] = useState(false)
    const [url, setUrl] = useState("")
    const apiUrl = useApiUrl()

    const { data: untypedData, setData } = useContext(ProviderDataContext)
    const data = untypedData as CollectionJson

    const args = useMemo(() => {
        if (!data)
            return null

        return data.arguments.images.map((imageUrl, index) => ({
            id: index,
            imageUrl: getImageUrl(apiUrl, imageUrl)
        }))
    }, [data])

    if (!args)
        return <Loader />


    const pickImage = async (multiple: boolean) => {
        // No permissions request is necessary for launching the image library
        let result = await ImagePicker.launchImageLibraryAsync({
            mediaTypes: ['images'],
            allowsEditing: !multiple,
            aspect: multiple ? undefined : [1, 1],
            allowsMultipleSelection: multiple
        });

        if (!result.canceled) {
            setUploading(true)
            const allResponses = Promise.all(result.assets.map(async e => {
                const res = await fetch(apiUrl + "/pixeljoint/upload_img", {
                    method: "POST",
                    body: formDataFromImagePicker(e),
                    headers: {
                        Accept: "application/json",
                    },
                })

                return (await res.json()) as UploadImgResponse
            })).finally(() => setUploading(false))
            return (await allResponses).map(e => e.path)
        }

        return null
    };

    const pickImageWrapper = (multiple: boolean) => {
        pickImage(multiple).then((paths) => {
            if (!paths)
                return

            setData(e => {
                const copy = JSON.parse(JSON.stringify(e)) as CollectionJson
                paths.forEach(path => copy.arguments.images.push(`file://${path}`))

                return copy
            })
        }).catch(e => {
            Toast.show({
                type: "error",
                text1: "Error uploading image",
                text2: e.message ?? JSON.stringify(e)
            })
        })
    }

    return <>
        <AlertDialog open={isDialogOpen} onOpenChange={e => setIsDialogOpen(e)}>
            <AlertDialogContent>
                <AlertDialogHeader>
                    <AlertDialogTitle>Input URL to add</AlertDialogTitle>
                </AlertDialogHeader>
                <Input
                    placeholder='Enter URL here'
                    value={url}
                    onChangeText={setUrl}
                    aria-labelledby='inputLabel'
                    aria-errormessage='inputError'
                    keyboardType='url'
                    className="w-72"
                />
                <AlertDialogFooter>
                    <AlertDialogCancel onPress={() => setIsDialogOpen(false)}>
                        <Text>Cancel</Text>
                    </AlertDialogCancel>
                    <AlertDialogAction onPress={() => {
                        if (!url || !isValidHttpUrl(url)) {
                            Toast.show({
                                type: "error",
                                text1: "Invalid URL",
                                text2: "Please enter a valid URL"
                            })
                            return
                        }

                        setData(e => {
                            const copy = JSON.parse(JSON.stringify(e)) as CollectionJson
                            copy.arguments.images.push(url)

                            return copy
                        })

                        setUrl("")
                        setIsDialogOpen(false)
                    }}>
                        <Text>Add</Text>
                    </AlertDialogAction>
                </AlertDialogFooter>
            </AlertDialogContent>
        </AlertDialog>

        <View className='w-full flex-1 flex-row flex-wrap p-10 gap-5'>
            {args.map((e, i) => {
                return <CollectionItem key={`${e.id}-${e.imageUrl}`} item={e} index={i} />
            })}

            <DropdownMenu>
                <DropdownMenuTrigger asChild>
                    <Button
                        variant="outline"
                        size={null}
                        disabled={uploading}
                        className='justify-center items-center text-slate-800 shadow p-3 border-2 border-dashed h-[128px] w-[128px]'
                    >
                        {uploading ? <Loader /> : <Plus className="text-foreground" />}
                    </Button>
                </DropdownMenuTrigger>
                <DropdownMenuContent className="w-36 native:w-48 p-3">
                    <DropdownMenuItem onPress={() => pickImageWrapper(false)}>
                        <FilePlus2 className="text-foreground" />
                        <Text>File</Text>
                    </DropdownMenuItem>
                    <DropdownMenuItem onPress={() => pickImageWrapper(true)}>
                        <FilePlus2 className="text-foreground" />
                        <Text>File (Multiple)</Text>
                    </DropdownMenuItem>
                    <DropdownMenuItem onPress={() => setIsDialogOpen(true)}>
                        <Paperclip className='text-foreground' />
                        <Text>URL</Text>
                    </DropdownMenuItem>
                </DropdownMenuContent>
            </DropdownMenu>
        </View>
    </>
}