import React, { createContext, useContext } from 'react';
import { ProviderValue } from '../apiTypes/list_scenes';
import { ReactSetState } from '~/lib/utils';


export type ProviderDataState = {
    data: ProviderValue | null,
    setData: ReactSetState<ProviderValue>
}

export const ProviderDataContext = createContext<ProviderDataState>(null as any);

export function ProviderDataProvider({ data, setData, children }: React.PropsWithChildren<{
    data: ProviderValue,
    setData: ReactSetState<ProviderValue>
}>) {
    return <ProviderDataContext.Provider value={{
        data,
        setData
    }}>
        {children}
    </ProviderDataContext.Provider>
}

export function useProviderData<T>() {
    const data = useContext(ProviderDataContext).data as T;

    return data
}