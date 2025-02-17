export interface ListScenes {
    name:       string;
    properties: Property<any>[];
}

export interface Property<T> {
    default_value: T;
    name:          string;
}


export type CollectionProvider = {
    type: "collection";
    arguments: string[]
}

export type PagesProvider = {
    type: "pages";
    arguments: {
        begin: number,
        end: number
    }
}

export type ProviderValue = CollectionProvider | PagesProvider;