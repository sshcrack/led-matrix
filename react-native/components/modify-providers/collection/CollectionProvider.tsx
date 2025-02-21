import * as ImagePicker from 'expo-image-picker';
import { useContext, useMemo, useState } from 'react';
import { View } from 'react-native';
import Toast from 'react-native-toast-message';
import { UploadImgResponse } from '~/components/apiTypes/pixeljoint/upload_img';
import Loader from '~/components/Loader';
import { Button } from '~/components/ui/button';
import { getApiUrl } from '~/components/useFetch';
import { Plus } from '~/lib/icons/Plus';
import { getImageUrl } from '~/lib/utils';
import { CollectionProvider as CollectionJson } from '../../apiTypes/list_scenes';
import { ProviderDataContext } from '../ProviderDataContext';
import CollectionItem from './CollectionItem';
import { useSubConfig } from '~/components/configShare/ConfigProvider';

export interface DataProp {
    id: number;
    imageUrl: string;
}


function formDataFromImagePicker(result: ImagePicker.ImagePickerSuccessResult) {
    const formData = new FormData();

    const asset = result.assets[0]

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

    const { data: untypedData, setData } = useContext(ProviderDataContext)
    const data = untypedData as CollectionJson

    const args = useMemo(() => {
        if (!data)
            return null

        return data.arguments.map((imageUrl, index) => ({
            id: index,
            imageUrl: getImageUrl(imageUrl)
        }))
    }, [data])

    if (!args)
        return <Loader />


    const pickImage = async () => {
        // No permissions request is necessary for launching the image library
        let result = await ImagePicker.launchImageLibraryAsync({
            mediaTypes: ['images'],
            allowsEditing: true,
            aspect: [1, 1]
        });

        if (!result.canceled) {
            setUploading(true)
            // Upload the image to the API route.
            const response = await fetch(getApiUrl("/pixeljoint/upload_img"), {
                method: "POST",
                body: formDataFromImagePicker(result),
                headers: {
                    Accept: "application/json",
                },
            }).finally(() => setUploading(false));

            const res: UploadImgResponse = await response.json();
            return res.path
        }

        return null
    };


    return <View className='w-full flex-1 flex-row flex-wrap p-10 gap-5'>
        {args.map((e, i) => {
            return <CollectionItem key={`${e.id}-${e.imageUrl}`} item={e} index={i} />
        })}
        <Button
            onPress={() => {
                pickImage().then((path) => {
                    if (!path)
                        return

                    setData(e => {
                        const copy = JSON.parse(JSON.stringify(e)) as CollectionJson
                        copy.arguments.push(`file://${path}`)

                        return copy
                    })
                }).catch(e => {
                    Toast.show({
                        type: "error",
                        text1: "Error uploading image",
                        text2: e.message ?? JSON.stringify(e)
                    })
                })
            }}
            variant="outline"
            size={null}
            className='justify-center items-center text-slate-800 shadow p-3 border-2 border-dashed h-[128px] w-[128px]'
        >
            {uploading ? <Loader /> : <Plus className="text-foreground" />}
        </Button>
    </View>
}