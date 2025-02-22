import { useContext, useState } from 'react';
import Toast from 'react-native-toast-message';
import { Save } from '~/lib/icons/Save';
import Loader from '../Loader';
import { Button } from '../ui/button';
import { getApiUrl } from '../useFetch';
import { ConfigContext } from './ConfigProvider';

export default function SaveButton({ presetId}: { presetId: string}) {
    const { config } = useContext(ConfigContext)
    const preset = config.get(presetId)
    const [isSaving, setIsSaving] = useState(false)

    return <Button
        size="icon"
        variant="secondary"
        className='p-3'
        disabled={isSaving}
        onPress={() => {
            console.log("Saving preset with name", presetId, "and config", preset)
            setIsSaving(true)
            fetch(getApiUrl(`/presets?id=${presetId}`), {
                method: "POST",
                body: JSON.stringify(preset),
                headers: {
                    'Content-Type': 'application/json'
                }
            })
                .then(() => {
                    console.log("Successfully saved preset")
                })
                .catch(e => Toast.show({
                    type: "success",
                    text1: "Error saving preset",
                    text2: e.message ?? JSON.stringify(e)
                }))
                .finally(() => setIsSaving(false))
        }}
    >
        {isSaving ? <Loader /> : <Save className='text-foreground' />}
    </Button>
}