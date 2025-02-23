import { useContext } from 'react';
import { Pressable } from 'react-native';
import { ApiUrlContext } from '../apiUrl/ApiUrlProvider';
import { RotateCcw } from '~/lib/icons/RotateCcw';

export default function ResetApiUrl() {
    const { setApiUrl } = useContext(ApiUrlContext)

    return <Pressable
        onPress={() => {
            setApiUrl(null)
            console.log("Setting null")
        }}
        className='pr-5 p-2 pl-0 items-center justify-center'
    >
        <RotateCcw  size={24} strokeWidth={1.25} className='text-foreground' />
    </Pressable>
}