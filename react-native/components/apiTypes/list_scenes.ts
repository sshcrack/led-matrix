export interface ListScenes {
    name: string;
    properties: Property<any>[];
}

export interface Property<T> {
    default_value: T;
    name: string;
    type_id: TypeId
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

export type TypeId = "string" | "int" | "double" | "bool" | "float" | "millis" | "json" | "int16_t"