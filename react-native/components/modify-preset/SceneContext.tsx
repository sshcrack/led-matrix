import { createContext, useCallback, useContext } from 'react';

type SceneContextType = {
    sceneId: string,
    updateProperty: (propertyName: string, value: any) => void;
};

export const SceneContext = createContext<SceneContextType | null>(null);

export function useSceneContext() {
    const context = useContext(SceneContext);
    if (!context) {
        throw new Error('useSceneContext must be used within a SceneContextProvider');
    }
    return context;
}

export function usePropertyUpdate(propertyName: string) {
    const { updateProperty } = useSceneContext();
    return useCallback((value: any) => {
        updateProperty(propertyName, value);
    }, [propertyName, updateProperty]);
}
