import { useContext, useState } from 'react';
import Toast from 'react-native-toast-message';
import { Save } from '~/lib/icons/Save';
import Loader from '../Loader';
import { Button } from '../ui/button';
import { getApiUrl } from '../useFetch';
import { ConfigContext } from './ConfigProvider';
import { objectToArrayPresets } from '../apiTypes/list_presets';

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
            console.log("Saving preset with name", presetId, "and config", JSON.stringify(preset, null, 2))
            setIsSaving(true)
            const raw = objectToArrayPresets(preset!)
            fetch(getApiUrl(`/preset?id=${presetId}`), {
                method: "POST",
                body: JSON.stringify(raw),
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