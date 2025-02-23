import { useState } from 'react';
import { Plus } from '~/lib/icons/Plus';
import { useApiUrl } from '../apiUrl/ApiUrlProvider';
import Loader from '../Loader';
import { AlertDialog, AlertDialogCancel, AlertDialogContent, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle, AlertDialogTrigger } from '../ui/alert-dialog';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Text } from '../ui/text';

export default function AddPresetButton({ presetNames, setRetry }: { presetNames: string[], setRetry: () => void }) {
    const apiUrl = useApiUrl()
    const [presetName, setPreset] = useState<string>("")
    const [adding, setAdding] = useState(false)
    const [open, setOpen] = useState(false)
    const [errorText, setErrorText] = useState<string | null>(null)


    return <AlertDialog open={open} onOpenChange={setOpen}>
        <AlertDialogTrigger asChild>
            <Button variant="outline" size={null} className="w-[20rem] h-[10rem] justify-center items-center text-slate-800 p-3 border-[3px] border-dashed ">
                {adding ? <Loader />
                    : <Plus className="text-foreground" />
                }
            </Button>
        </AlertDialogTrigger>
        <AlertDialogContent className='w-3/4'>
            <AlertDialogHeader>
                <AlertDialogTitle>Add Preset</AlertDialogTitle>
            </AlertDialogHeader>
            <Input value={presetName} onChangeText={setPreset} className='w-full' placeholder='Enter name here' />
            {errorText && <Text className='text-red-500'>{errorText}</Text>}
            <AlertDialogFooter>
                <AlertDialogCancel>
                    <Text>Cancel</Text>
                </AlertDialogCancel>
                <Button onPress={() => {
                    if (presetNames.includes(presetName)) {
                        setErrorText("Preset already exists")
                        return
                    }

                    setAdding(true)
                    fetch(apiUrl + `/add_preset?id=${encodeURIComponent(presetName)}`, {
                        method: "POST",
                        body: JSON.stringify({ scenes: [] }),
                        headers: {
                            'Content-Type': 'application/json'
                        }
                    })
                        .then(async e => {
                            if (!e.ok)
                                throw new Error("Failed to add preset: " + (await e.json().catch(e => ({ error: "Unknown error" })))?.error)
                        })
                        .then(() => {
                            presetNames.push(presetName)
                            setPreset("")
                            setOpen(false)
                            setRetry()
                        })
                        .catch(e => {
                            setErrorText(`Error adding preset: ${e.message}`)
                        })
                        .finally(() => {
                            setAdding(false)
                        })
                }}>
                    {adding ? <Loader /> : <Text>Add</Text>}
                </Button>
            </AlertDialogFooter>
        </AlertDialogContent>
    </AlertDialog>
}