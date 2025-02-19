import React, { createContext, useContext } from 'react';
import { ProviderValue } from '../apiTypes/list_scenes';


const ProviderDataContext = createContext<ProviderValue | null>(null);

export function ProviderDataProvider({ data, children }: React.PropsWithChildren<{ data: ProviderValue }>) {
    return <ProviderDataContext.Provider value={data}>
        {children}
    </ProviderDataContext.Provider>
}

export function useProviderData<T>() {
    const context = useContext(ProviderDataContext) as T;

    return context
}