import AsyncStorage from '@react-native-async-storage/async-storage';
import { createContext, useContext, useEffect, useRef, useState } from 'react';
import Toast from 'react-native-toast-message';
import { ReactSetState } from '~/lib/utils';
import { Text } from '../ui/text';
import { ActivityIndicator, Platform, SafeAreaView } from 'react-native';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Input } from '../ui/input';
import { View } from 'react-native';
import { Button } from '../ui/button';
import Loader from '../Loader';

export type ApiUrlState = {
    apiUrl: string | null,
    setApiUrl: ReactSetState<string | null>
}

export const ApiUrlContext = createContext<ApiUrlState>({} as ApiUrlState)

export function useApiUrl(path?: string) {
    const { apiUrl } = useContext(ApiUrlContext)
    return apiUrl + (path ?? "")
}

export function ApiUrlProvider({ children }: React.PropsWithChildren<{}>) {
    const [apiUrl, setApiUrl] = useState<string | null>(null)
    const [checkingStatus, setCheckingStatus] = useState(false)
    const [inputVal, setInputVal] = useState("")

    useEffect(() => {
        if(!apiUrl || (Platform.OS === "web" && !__DEV__))
            return

        AsyncStorage.setItem("apiUrl", apiUrl)
            .catch(e => {
                console.error("Error setting apiUrl", e)
                Toast.show({
                    type: "error",
                    text1: "Error setting apiUrl",
                    text2: e.message
                })
            })

    }, [apiUrl])

    useEffect(() => {
        if(Platform.OS === "web" && !__DEV__) {
            setApiUrl(window.location.protocol + "//" + window.location.host)
            return;
        }

        AsyncStorage.getItem("apiUrl").then(e => {
            setApiUrl(e)
        })
            .catch(e => {
                console.error("Error getting apiUrl", e)
                Toast.show({
                    type: "error",
                    text1: "Error getting apiUrl",
                    text2: e.message
                })
            })
    }, [])

    return <ApiUrlContext.Provider value={{
        apiUrl,
        setApiUrl
    }}>
        {
            apiUrl ? children : (
                <SafeAreaProvider >
                    <SafeAreaView className="flex-1 gap-5 justify-center items-center">
                        <Text className='text-xl'>Set API URL of the LED matrix</Text>
                        <View className='w-3/4 flex-row'>
                            <Input
                                placeholder="http://192.168.178.5:8080"
                                className="rounded-r-none flex-1"
                                autoCapitalize='none'
                                value={inputVal}
                                onChangeText={setInputVal}
                                keyboardType='url'
                            />
                            <Button disabled={checkingStatus} className="rounded-l-none" onPress={() => {
                                setCheckingStatus(true)
                                fetch(inputVal + "/list_scenes")
                                    .then(() => {
                                        setApiUrl(inputVal)
                                    })
                                    .catch(e => {
                                        Toast.show({
                                            type: "error",
                                            text1: "Invalid URL",
                                            text2: e.message
                                        })
                                    })
                                    .finally(() => setCheckingStatus(false))
                            }}>
                                {!checkingStatus && <Text>Set</Text>}
                                {checkingStatus && (
                                    <View className='flex-row gap-2'>
                                        <Loader />
                                    </View>
                                )}
                            </Button>
                        </View>
                    </SafeAreaView>
                </SafeAreaProvider>
            )
        }
    </ApiUrlContext.Provider >
}